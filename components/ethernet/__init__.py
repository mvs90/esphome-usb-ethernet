# SPDX-License-Identifier: MIT
# USB-only overlay for ESPHome 2026.9.1; adapted from its Ethernet component.
from esphome import automation
from esphome.automation import Condition
import esphome.codegen as cg
from esphome.components.network import add_use_address, get_network_priority, ip_address_literal
import esphome.config_validation as cv
from esphome.const import (
    CONF_DNS1, CONF_DNS2, CONF_DOMAIN, CONF_ENABLE_ON_BOOT, CONF_GATEWAY,
    CONF_ID, CONF_MANUAL_IP, CONF_ON_CONNECT, CONF_ON_DISCONNECT,
    CONF_STATIC_IP, CONF_SUBNET, CONF_TYPE, CONF_USE_ADDRESS, __version__,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority
import esphome.final_validate as fv

AUTO_LOAD = ['network']
ETHERNET_IP_STATE_LISTENERS_KEY = 'ethernet_ip_state_listeners'
ethernet_ns = cg.esphome_ns.namespace('ethernet')
EthernetComponent = ethernet_ns.class_('EthernetComponent', cg.Component)
EthernetType = ethernet_ns.enum('EthernetType')
ManualIP = ethernet_ns.struct('ManualIP')
EthernetConnectedCondition = ethernet_ns.class_('EthernetConnectedCondition', Condition)
EthernetEnabledCondition = ethernet_ns.class_('EthernetEnabledCondition', Condition)
EthernetEnableAction = ethernet_ns.class_('EthernetEnableAction', automation.Action)
EthernetDisableAction = ethernet_ns.class_('EthernetDisableAction', automation.Action)


def request_ethernet_ip_state_listener():
    CORE.data[ETHERNET_IP_STATE_LISTENERS_KEY] = CORE.data.get(ETHERNET_IP_STATE_LISTENERS_KEY, 0) + 1


MANUAL_IP_SCHEMA = cv.Schema({
    cv.Required(CONF_STATIC_IP): cv.ipv4address,
    cv.Required(CONF_GATEWAY): cv.ipv4address,
    cv.Required(CONF_SUBNET): cv.ipv4address,
    cv.Optional(CONF_DNS1, default='0.0.0.0'): cv.ipv4address,
    cv.Optional(CONF_DNS2, default='0.0.0.0'): cv.ipv4address,
})
BASE_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(EthernetComponent),
    cv.Optional(CONF_MANUAL_IP): MANUAL_IP_SCHEMA,
    cv.Optional(CONF_DOMAIN, default='.local'): cv.domain_name,
    cv.Optional(CONF_USE_ADDRESS): cv.string_strict,
    cv.Optional(CONF_ENABLE_ON_BOOT, default=True): cv.boolean,
    cv.Optional('mac_address'): cv.mac_address,
    cv.Optional('enable_ipv4', default=True): cv.boolean,
    cv.Optional('realtek'): cv.Schema({
        cv.Optional('speed', default='AUTO'): cv.one_of('AUTO', '10', '100', upper=True),
        cv.Optional('duplex', default='FULL'): cv.one_of('FULL', 'HALF', upper=True),
        cv.Optional('eee'): cv.boolean,
        cv.Optional('wake_filters'): cv.int_range(min=0, max=31),
        cv.Optional('tx_checksum_offload', default=False): cv.boolean,
        cv.Optional('tx_aggregation', default=False): cv.boolean,
        cv.Optional('tcp_segmentation', default=False): cv.boolean,
        cv.Optional('rx_aggregation', default=False): cv.boolean,
    }),
    cv.Optional(CONF_ON_CONNECT): automation.validate_automation(single=True),
    cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation(single=True),
}).extend(cv.COMPONENT_SCHEMA)


def _validate_usb_ecm(config):
    from esphome.components.esp32 import get_esp32_variant
    if __version__ != '2026.9.1':
        raise cv.Invalid('This experimental overlay requires ESPHome 2026.9.1 (including its logger)')
    if not CORE.is_esp32 or get_esp32_variant() != 'ESP32S3' or not CORE.using_toolchain_esp_idf:
        raise cv.Invalid('USB ECM currently requires ESP32-S3 with native toolchain: esp-idf')
    if not config[CONF_ENABLE_ON_BOOT]:
        raise cv.Invalid('USB ECM currently requires enable_on_boot: true')
    if not config['enable_ipv4'] and CONF_MANUAL_IP in config:
        raise cv.Invalid('enable_ipv4: false cannot be combined with manual_ip')
    if 'mac_address' in config:
        parts = config['mac_address'].parts
        if parts[0] & 1 or not any(parts):
            raise cv.Invalid('mac_address must be a nonzero unicast address')
    if config.get('realtek', {}).get('speed') == 'AUTO' and config['realtek']['duplex'] != 'FULL':
        raise cv.Invalid('duplex HALF requires forced speed 10 or 100')
    for option in ('tx_checksum_offload', 'tx_aggregation', 'rx_aggregation', 'tcp_segmentation'):
        if config[CONF_TYPE] != 'USB_RTL8152' and config.get('realtek', {}).get(option):
            raise cv.Invalid(option + ' requires USB_RTL8152')
    config.setdefault(CONF_USE_ADDRESS, str(config[CONF_MANUAL_IP][CONF_STATIC_IP])
                      if CONF_MANUAL_IP in config else CORE.name + config[CONF_DOMAIN])
    return config


CONFIG_SCHEMA = cv.All(cv.typed_schema({'USB_ECM': BASE_SCHEMA, 'USB_RTL8152': BASE_SCHEMA}, upper=True), _validate_usb_ecm)


def _final_validate_usb(config):
    full = fv.full_config.get()
    if not config['enable_ipv4'] and not full.get('network', {}).get('enable_ipv6', False):
        raise cv.Invalid('enable_ipv4: false requires network.enable_ipv6: true')
    if 'usb_host' in full:
        raise cv.Invalid('USB ECM owns the sole USB host; a second usb_host is unsupported')
    if 'wifi' in full:
        raise cv.Invalid('This experimental USB ECM overlay requires Wi-Fi to be absent')
    if full.get('logger', {}).get('hardware_uart') != 'UART0':
        raise cv.Invalid('USB ECM requires logger hardware_uart: UART0; USB owns the shared PHY')


FINAL_VALIDATE_SCHEMA = _final_validate_usb


@coroutine_with_priority(CoroPriority.COMMUNICATION)
async def to_code(config):
    from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option, include_builtin_idf_component
    var = cg.new_Pvariable(config[CONF_ID])
    if (prio := get_network_priority('ethernet')) is not None:
        cg.set_setup_priority(var, prio)
    await cg.register_component(var, config)
    include_builtin_idf_component('esp_eth')
    add_idf_component(name='espressif/usb', ref='1.6.0')
    if config[CONF_TYPE] == 'USB_RTL8152':
        add_idf_component(name='espressif/usb_host_cdc_acm', ref='2.4.1')
    # Pin transitive dependencies as well: a future CDC release must not silently
    # alter the validated lifecycle or descriptor behavior.
    for name, version in [('iot_usbh_ecm', '0.4.0'), ('iot_usbh_cdc', '3.1.0'),
                          ('iot_eth', '1.1.0'), ('cmake_utilities', '1.1.1')]:
        add_idf_component(name='espressif/' + name, ref=version)
    add_idf_sdkconfig_option('CONFIG_USB_HOST_HUBS_SUPPORTED', True)
    add_idf_sdkconfig_option('CONFIG_ESP_NETIF_RECEIVE_REPORT_ERRORS', True)
    add_idf_sdkconfig_option('CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK', True)
    if config[CONF_TYPE] == 'USB_RTL8152':
        cg.add_define('USE_ETHERNET_REALTEK_BULK')
    cg.add_define('USE_ETHERNET_USB_ECM')
    cg.add_define('USE_ETHERNET')
    cg.add(var.set_type(EthernetType.ETHERNET_TYPE_USB_ECM))
    cg.add(var.set_ipv4_enabled(config['enable_ipv4']))
    add_use_address(var, config[CONF_USE_ADDRESS])
    if 'mac_address' in config:
        cg.add(var.set_fixed_mac(config['mac_address'].parts))
    if 'realtek' in config:
        r = config['realtek']
        cg.add(var.set_native_tx_offload(r['tx_checksum_offload']))
        cg.add(var.set_native_tso_on_boot(r['tcp_segmentation']))
        cg.add(var.set_native_aggregation(r['tx_aggregation'],r['rx_aggregation']))
        cg.add(var.set_realtek_options(0 if r['speed'] == 'AUTO' else int(r['speed']),
                                      r['duplex'] == 'FULL', int(r['eee']) if 'eee' in r else -1,
                                      r.get('wake_filters', -1)))
    if CONF_MANUAL_IP in config:
        cg.add_define('USE_ETHERNET_MANUAL_IP')
        fields = [('static_ip', CONF_STATIC_IP), ('gateway', CONF_GATEWAY), ('subnet', CONF_SUBNET),
                  ('dns1', CONF_DNS1), ('dns2', CONF_DNS2)]
        cg.add(var.set_manual_ip(cg.StructInitializer(ManualIP,
               *((field, ip_address_literal(config[CONF_MANUAL_IP][option])) for field, option in fields))))
    for option, define, trigger in [(CONF_ON_CONNECT, 'USE_ETHERNET_CONNECT_TRIGGER', var.get_connect_trigger),
                                     (CONF_ON_DISCONNECT, 'USE_ETHERNET_DISCONNECT_TRIGGER', var.get_disconnect_trigger)]:
        if option in config:
            cg.add_define(define)
            await automation.build_automation(trigger(), [], config[option])
    CORE.add_job(final_step)


@coroutine_with_priority(CoroPriority.FINAL)
async def final_step():
    if count := CORE.data.get(ETHERNET_IP_STATE_LISTENERS_KEY, 0):
        cg.add_define('USE_ETHERNET_IP_STATE_LISTENERS')
        cg.add_define('ESPHOME_ETHERNET_IP_STATE_LISTENERS', count)


async def _new_pvariable_to_code(config, id_, template_arg, args):
    return cg.new_Pvariable(id_, template_arg)


for _name, _cls in [('ethernet.connected', EthernetConnectedCondition), ('ethernet.enabled', EthernetEnabledCondition)]:
    automation.register_condition(_name, _cls, cv.Schema({}))(_new_pvariable_to_code)
for _name, _cls in [('ethernet.enable', EthernetEnableAction), ('ethernet.disable', EthernetDisableAction)]:
    automation.register_action(_name, _cls, cv.Schema({}), synchronous=True)(_new_pvariable_to_code)


def _validate_advertisement(value):
    if value & ~0x01e0:
        raise cv.Invalid('Unsupported 10/100 advertisement bits')
    return value


RealtekControlAction = ethernet_ns.class_('RealtekControlAction', automation.Action)


@automation.register_action('ethernet.realtek.link', RealtekControlAction, cv.Schema({
    cv.Optional('speed', default='AUTO'): cv.one_of('AUTO', '10', '100', upper=True),
    cv.Optional('duplex', default='FULL'): cv.one_of('FULL', 'HALF', upper=True),
    cv.Optional('advertisement', default=480): cv.All(cv.int_range(min=32, max=480),
        _validate_advertisement),
}), synchronous=True)
async def realtek_link_action(config, action_id, template_arg, args):
    if config['speed'] == 'AUTO' and config['duplex'] != 'FULL':
        raise cv.Invalid('duplex HALF requires forced speed 10 or 100')
    var = cg.new_Pvariable(action_id, template_arg)
    cg.add(var.set_parameters(0, config['advertisement'], config['speed'] == 'AUTO',
                              100 if config['speed'] == 'AUTO' else int(config['speed']), config['duplex'] == 'FULL'))
    return var


for action_name, operation, schema in [
    ('eee', 1, {'enabled': cv.boolean}),
    ('wake', 2, {'filters': cv.int_range(min=0, max=31)}),
    ('pause', 3, {'rx': cv.boolean, 'tx': cv.boolean}),
    ('promiscuous', 4, {'enabled': cv.boolean}),
    ('tx_checksum_offload', 7, {'enabled': cv.boolean}),
    ('tcp_segmentation', 9, {'enabled': cv.boolean}),
]:
    def register_realtek_action(name, op, fields):
        @automation.register_action('ethernet.realtek.' + name, RealtekControlAction,
                                    cv.Schema({cv.Required(k): v for k, v in fields.items()}), synchronous=True)
        async def to_action(config, action_id, template_arg, args):
            var = cg.new_Pvariable(action_id, template_arg)
            values = [config[key] for key in fields]
            cg.add(var.set_parameters(op, int(values[0]), int(values[1]) if len(values) > 1 else 0, 100, 1))
            return var
    register_realtek_action(action_name, operation, schema)


def _led_mode(value):
    if value & ~0x0b:
        raise cv.Invalid('LED bits: link10=1, link100=2, activity=8')
    return value


@automation.register_action('ethernet.realtek.leds', RealtekControlAction, cv.Schema({
    cv.Required('led0'): cv.All(cv.int_range(min=0, max=11), _led_mode),
    cv.Required('led1'): cv.All(cv.int_range(min=0, max=11), _led_mode),
    cv.Optional('period', default=1): cv.int_range(min=0, max=3),
    cv.Optional('duty', default=2): cv.int_range(min=0, max=3),
}), synchronous=True)
async def realtek_led_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    cg.add(var.set_parameters(5, config['led0'], config['led1'], config['period'], config['duty']))
    return var


def _validate_suspend(config):
    if 'wake_filters' in config and not config['remote_wake']:
        raise cv.Invalid('wake_filters requires remote_wake: true')
    ms = config['maximum_duration'].total_milliseconds
    if ms and ms < 1000:
        raise cv.Invalid('maximum_duration must be 0s or at least 1s')
    if not ms and config['remote_wake'] and config.get('wake_filters', 1) == 0:
        raise cv.Invalid('Indefinite USB suspend requires at least one wake filter')
    if not ms and not config['remote_wake']:
        raise cv.Invalid('Indefinite USB suspend requires remote_wake: true')
    return config


@automation.register_action('ethernet.realtek.usb_suspend', RealtekControlAction, cv.All(cv.Schema({
    cv.Optional('maximum_duration', default='5s'): cv.All(cv.positive_time_period_milliseconds, cv.Range(min=cv.TimePeriod(milliseconds=0), max=cv.TimePeriod(milliseconds=86400000))),
    cv.Optional('remote_wake', default=False): cv.boolean,
    cv.Optional('wake_filters'): cv.int_range(min=0, max=31),
}), _validate_suspend), synchronous=True)
async def realtek_suspend_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    cg.add(var.set_parameters(6, config['maximum_duration'].total_milliseconds, config['remote_wake'], config.get('wake_filters', 1), 0))
    return var


@automation.register_action('ethernet.realtek.usb_resume', RealtekControlAction, cv.Schema({}), synchronous=True)
async def realtek_resume_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    cg.add(var.set_parameters(8, 0, 0, 0, 0))
    return var


RealtekMulticastAction = ethernet_ns.class_('RealtekMulticastAction', automation.Action)

def _multicast_mac(value):
    value = cv.mac_address(value)
    if not value.parts[0] & 1 or all(p == 255 for p in value.parts):
        raise cv.Invalid('addresses must contain multicast MACs, excluding broadcast')
    return value

def _multicast_config(config):
    if config['mode'] == 'ALL' and config['addresses']:
        raise cv.Invalid('addresses require mode: FILTERED')
    return config

@automation.register_action('ethernet.realtek.multicast', RealtekMulticastAction, cv.All(cv.Schema({
    cv.Optional('mode', default='ALL'): cv.one_of('ALL', 'FILTERED', upper=True),
    cv.Optional('addresses', default=[]): cv.All(cv.ensure_list(_multicast_mac), cv.Length(max=64)),
    cv.Optional('preserve_basic_groups', default=True): cv.boolean,
}), _multicast_config), synchronous=True)
async def realtek_multicast_action(config, action_id, template_arg, args):
    import binascii
    bitmap = (1 << 64) - 1 if config['mode'] == 'ALL' else 0
    for mac in config['addresses']:
        raw = (~binascii.crc32(bytes(mac.parts))) & 0xffffffff
        index = int(f'{raw:032b}'[::-1], 2) >> 26
        bitmap |= 1 << index
    var = cg.new_Pvariable(action_id, template_arg)
    cg.add(var.set_parameters(cg.RawExpression(f'0x{bitmap:016X}ULL'), config['preserve_basic_groups']))
    return var
