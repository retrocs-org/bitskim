"""Private D-Bus notification receiver for actual desktop action tests."""
import json
from pathlib import Path
import sys
import gi
from gi.repository import Gio, GLib

root = Path(sys.argv[1])
connection = Gio.bus_get_sync(Gio.BusType.SESSION, None)
xml = '''<node>
<interface name="org.freedesktop.Notifications">
<method name="GetCapabilities"><arg type="as" direction="out"/></method>
<method name="GetServerInformation"><arg type="s" direction="out"/><arg type="s" direction="out"/><arg type="s" direction="out"/><arg type="s" direction="out"/></method>
<method name="Notify"><arg type="s" direction="in"/><arg type="u" direction="in"/><arg type="s" direction="in"/><arg type="s" direction="in"/><arg type="s" direction="in"/><arg type="as" direction="in"/><arg type="a{sv}" direction="in"/><arg type="i" direction="in"/><arg type="u" direction="out"/></method>
<method name="CloseNotification"><arg type="u" direction="in"/></method>
<signal name="ActionInvoked"><arg type="u"/><arg type="s"/></signal>
<signal name="NotificationClosed"><arg type="u"/><arg type="u"/></signal>
</interface>
<interface name="org.bitskim.NotificationChecks"><method name="Invoke"><arg type="u" direction="in"/><arg type="s" direction="in"/></method></interface>
</node>'''
records = []

def call(bus, sender, path, interface, method, parameters, invocation):
    values = parameters.unpack()
    if method == 'GetCapabilities':
        invocation.return_value(GLib.Variant('(as)', (['actions', 'body', 'body-markup'],)))
    elif method == 'GetServerInformation':
        invocation.return_value(GLib.Variant('(ssss)', ('BitSkim test receiver', 'BitSkim', '1', '1.2')))
    elif method == 'Notify':
        app, replacement, icon, title, body, actions, hints, expiry = values
        identifier = replacement or len(records) + 1
        records.append(dict(id=identifier, title=title, body=body, actions=actions))
        (root/'notifications.json').write_text(json.dumps(records))
        invocation.return_value(GLib.Variant('(u)', (identifier,)))
    elif method == 'Invoke':
        bus.emit_signal(None, path, 'org.freedesktop.Notifications', 'ActionInvoked', GLib.Variant('(us)', values))
        invocation.return_value(None)
    elif method == 'CloseNotification':
        bus.emit_signal(None, path, 'org.freedesktop.Notifications', 'NotificationClosed', GLib.Variant('(uu)', (values[0], 3)))
        invocation.return_value(None)

for interface in Gio.DBusNodeInfo.new_for_xml(xml).interfaces:
    connection.register_object('/org/freedesktop/Notifications', interface, call, None, None)
connection.call_sync('org.freedesktop.DBus', '/org/freedesktop/DBus', 'org.freedesktop.DBus', 'RequestName', GLib.Variant('(su)', ('org.freedesktop.Notifications', 0)), GLib.VariantType.new('(u)'), Gio.DBusCallFlags.NONE, 3000, None)
(root/'notifications-ready').write_text('ready')
GLib.MainLoop().run()
