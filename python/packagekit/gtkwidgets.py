"""
This module provides widgets to use PackageKit in a GTK application.
"""
#Copyright (C) 2008 Sebastian Heinlein <sevel@glatzor.de>

#Licensed under the GNU General Public License Version 2

#This program is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License as published by
#the Free Software Foundation; either version 2 of the License, or
#(at your option) any later version.

#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.

#You should have received a copy of the GNU General Public License
#along with this program; if not, write to the Free Software
#Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

__author__ = "Sebastian Heinlein <devel@glatzor.de>"

import gobject
import gtk
import pango
import pygtk
from gettext import gettext as _

import dbus

from packagekit.misc import PackageKitPackage
from packagekit.enums import *

from genums import *

import dbus.mainloop.glib
dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)

(COLUMN_ID,
 COLUMN_PACKAGE) = range(2)


class PackageKitStatusIcon(gtk.Image):
    """
    Provides a gtk.Image which shows an icon representing the status of a
    PackageKit transaction
    """
    def __init__(self, transaction=None, size=gtk.ICON_SIZE_DIALOG):
        gtk.Image.__init__(self)
        self.icon_size = size
        self.icon_name = None
        self._transaction = None
        self._signals = []
        self.set_alignment(0, 0)
        if transaction != None:
            self.set_transaction(transaction)

    def set_transaction(self, transaction):
        """Connect to the given transaction"""
        for s in self._signals: s.remove
        self._signals = []
        self._signals.append(
            transaction.connect_to_signal("StatusChanged",
                                          self._on_status_changed))
        self._transaction = transaction

    def set_icon_size(self, size):
        """Set the icon size to gtk stock icon size value"""
        self.icon_size = size

    def _on_status_changed(self, status):
        """Set the status icon according to the changed status"""
        icon_name = get_status_icon_name_from_enum(status)
        if icon_name != self.icon_name:
            self.set_from_icon_name(icon_name, self.icon_size)
            self.icon_name = icon_name


class PackageKitStatusAnimation(PackageKitStatusIcon):
    """
    Provides a gtk.Image which shows an animation representing the 
    transaction status
    """
    def __init__(self, transaction=None):
        PackageKitStatusIcon.__init__(self, transaction)
        self.animation = []
        self.ticker = 0
        self.animation_name = None
        self.frame_counter = 0
        self.iter = 0

    def set_animation(self, name, size=None, fallback=None):
        """Show and start the animation of the given name and size"""
        if name == self.animation_name:
            return
        if size != None:
            self.icon_size = size
        self.stop_animation()
        animation = []
        (width, height) = gtk.icon_size_lookup(self.icon_size)
        theme = gtk.icon_theme_get_default()
        theme.append_search_path("/usr/share/gnome-packagekit/icons")
        if theme.has_icon(name):
            pixbuf = theme.load_icon(name, width, 0)
            rows = pixbuf.get_height() / height
            cols = pixbuf.get_width() / width
            for r in range(rows):
                for c in range(cols):
                    animation.append(pixbuf.subpixbuf(c * width, r * height, 
                                                      width, height))
            if len(animation) > 0:
                self.animation = animation
                self.iter = 0
                self.set_from_pixbuf(self.animation[0])
                self.start_animation()
            else:
                self.set_from_pixbuf(pixbuf)
            self.icon_name = name
        else:
            self.set_from_icon_name(fallback, self.icon_size)
            self.icon_name = fallback

    def start_animation(self):
        """Start the animation"""
        if self.ticker == 0:
            self.ticker = gobject.timeout_add(200, self._advance)

    def stop_animation(self):
        """Stop the animation"""
        if self.ticker != 0:
            gobject.source_remove(self.ticker)
            self.ticker = 0

    def _advance(self):
        """
        Show the next frame of the animation and stop the animation if the
        widget is no longer visible
        """
        if self.get_property("visible") == False:
            self.ticker = 0
            return False
        self.iter = self.iter + 1
        if self.iter >= len(self.animation):
            self.iter = 0
        self.set_from_pixbuf(self.animation[self.iter])
        return True

    def _on_status_changed(self, status):
        """
        Set the animation according to the changed status
        """
        self.set_animation(name=get_status_animation_name_from_enum(status),
                           fallback=get_status_icon_name_from_enum(status))


class PackageKitStatusLabel(gtk.Label):
    """
    Status label for the running PackageKit transaction
    """
    def __init__(self, transaction=None):
        gtk.Label.__init__(self)
        self.set_alignment(0, 0)
        self.set_ellipsize(pango.ELLIPSIZE_END)
        self._transaction = None
        self._signals = []
        if transaction != None:
            self.set_transaction(transaction)

    def set_transaction(self, transaction):
        """Connect the status label to the given PackageKit transaction"""
        for h in self._signals:
            h.remove()
        self._signals = []
        self._signals.append(
            transaction.connect_to_signal("StatusChanged",
                                          self._on_status_changed))
        self._transaction = transaction

    def _on_status_changed(self, status):
        """Set the status text according to the changed status"""
        self.set_markup("<i>%s</i>" % get_status_string_from_enum(status))


class PackageKitProgressBar(gtk.ProgressBar):
    """
    Provides a gtk.Progress which represents the progress of a PackageKit
    transactions
    """
    def __init__(self, transaction=None):
        gtk.ProgressBar.__init__(self)
        self._show_time = False
        self._transaction = None
        self._signals = []
        if transaction != None:
            self.set_transaction(transaction)

    def set_transaction(self, transaction):
        """Connect the progress bar to the given PackageKit transaction"""
        for h in self._signals:
            h.remove()
        self._signals = []
        self._signals.append(
            transaction.connect_to_signal("ProgressChanged",
                                          self._on_progress_changed))
        self._transaction = transaction

    def set_show_time(self, enabled):
        """Show the elapsed and remaining time on the progress bar"""
        self._show_time = enabled
        if time == False:
            self.set_text("")

    def get_show_time(self):
        """
        Return True if the elapsed and remaining time are shown in the
        progress bar
        """
        return self._show_time

    def _on_progress_changed(self, progress, subprogress, elapsed, estimated):
        """
        Update the progress according to the latest progress information
        """
        if progress > 100:
            self.pulse()
        else:
            self.set_fraction(progress/100.0)
        if self._show_time:
            #FIXME: should be nicer
            self.set_text("elapsed: %s - estimated: %s" % (elapsed, estimated))


class PackageKitCancelButton(gtk.Button):
    """
    Provides a gtk.Button which allows to cancel a running PackageKit 
    transaction
    """
    def __init__(self, transaction):
        gtk.Button.__init__(self, stock=gtk.STOCK_CANCEL)
        self.set_sensitive(False)
        self._transaction = None
        self._signals = []
        if transaction != None:
            self.set_transaction(transaction)

    def set_transaction(self, transaction):
        """Connect the status label to the given PackageKit transaction"""
        for h in self._signals:
            h.remove()
        self._signals = []
        self._signals.append(
                transaction.connect_to_signal("Finished",
                                              self._on_finished))
        self._signals.append(
                transaction.connect_to_signal("AllowCancel",
                                              self._on_allow_cancel_changed))
        self._transaction = transaction
        self.connect("clicked", self._on_clicked, transaction)

    def _on_allow_cancel_changed(self, allow_cancel):
        """
        Enable the button if cancel is allowed and disable it in the other case
        """
        self.set_sensitive(allow_cancel)

    def _on_finished(self, transaction, status):
        self.set_sensitive(False)

    def _on_clicked(self, button, transaction):
        transaction.Cancel()


class PackageKitProgressDialog(gtk.Dialog):
    """
    Complete progress dialog for long taking PackageKit transactions, which
    features a progress bar, cancel button, status icon and label
    """
    def __init__(self, transaction=None, parent=None):
        gtk.Dialog.__init__(self, buttons=None, parent=parent)
        # Setup the dialog
        self.set_border_width(6)
        self.set_has_separator(False)
        self.set_resizable(False)
        self.vbox.set_spacing(6)
        # Setup the cancel button
        self.button_cancel = PackageKitCancelButton(transaction)
        self.add_action_widget(self.button_cancel, gtk.RESPONSE_CANCEL)
        # Setup the status icon, label and progressbar
        hbox = gtk.HBox()
        hbox.set_spacing(12)
        hbox.set_border_width(6)
        self.icon = PackageKitStatusAnimation()
        self.icon._on_status_changed(STATUS_WAIT)
        hbox.pack_start(self.icon, False, True, 0)
        vbox = gtk.VBox()
        vbox.set_spacing(12)
        self.label_role = gtk.Label()
        self.label_role.set_alignment(0, 0)
        vbox.pack_start(self.label_role, False, True, 0)
        vbox_progress = gtk.VBox()
        vbox_progress.set_spacing(6)
        self.progress = PackageKitProgressBar()
        vbox_progress.pack_start(self.progress, False, True, 0)
        self.label = PackageKitStatusLabel()
        self.label._on_status_changed(STATUS_WAIT)
        vbox_progress.pack_start(self.label, False, True, 0)
        vbox.pack_start(vbox_progress, False, True, 0)
        hbox.pack_start(vbox, True, True, 0)
        self.vbox.pack_start(hbox, True, True, 0)
        self._transaction = None
        self._signals = []
        self.set_title("")
        self.realize()
        self.progress.set_size_request(250, -1)
        self.window.set_functions(gtk.gdk.FUNC_MOVE)

        if transaction != None:
            self.set_transaction(transaction)

    def run(self):
        """Run the transaction"""
        parent = self.get_transient_for()
        if parent:
            parent.set_sensitive(False)
            parent.window.set_cursor(gtk.gdk.Cursor(gtk.gdk.WATCH))
        self._transaction.run()
        role_enum = self._transaction._iface.GetRole()[0]
        role = get_role_localised_present_from_enum(role_enum)
        self.set_title(role)
        self.label_role.set_markup("<big><b>%s</b></big>" % role)
        self.show_all()
        gtk.Dialog.run(self)
        if parent:
            parent.set_sensitive(True)
            parent.window.set_cursor(None)

    def set_transaction(self, transaction):
        """Connect the dialog to the given PackageKit transaction"""
        for h in self._signals:
            h.remove()
        self._signals = []
        self._signals.append(
            transaction.connect_to_signal("Finished",self._on_finished))
        self.progress.set_transaction(transaction)
        self.icon.set_transaction(transaction)
        self.label.set_transaction(transaction)
        self._transaction = transaction

    def _on_finished(self, status, runtime):
        self.response(gtk.RESPONSE_CLOSE)

class PackageKitMessageDialog(gtk.MessageDialog):
    """
    Dialog for PackageKit errors and messages with details in an expandable
    text view
    """
    def __init__(self, enum, parent=None, type=gtk.MESSAGE_ERROR, details=None):
        gtk.MessageDialog.__init__(self, parent=parent,
                                   type=type, buttons=gtk.BUTTONS_CLOSE)
        if type == gtk.MESSAGE_INFO:
            text = get_message_string_from_enum(enum)
            self.set_markup("<big><b>%s</b></big>" % text)
        else:
            text = get_error_string_from_enum(enum)
            desc = get_error_description_from_enum(enum)
            self.set_markup("<big><b>%s</b></big>\n\n%s" % (text, desc))
        if details:
            #TRANSLATORS: expander label in the error dialog
            expander = gtk.expander_new_with_mnemonic(_("_Details"))
            expander.set_spacing(6)
            scrolled = gtk.ScrolledWindow()
            scrolled.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
            scrolled.set_shadow_type(gtk.SHADOW_ETCHED_IN)
            textview = gtk.TextView()
            buffer = textview.get_buffer()
            buffer.insert_at_cursor(details)
            scrolled.add(textview)
            expander.add(scrolled)
            box = self.label.get_parent()
            box.add(expander)
            self.show_all()


class PackageListStore(gtk.ListStore):
    """
    Provides a gtk.ListStore which can handle packages returned by a
    PackageKit transaction.

    The first column contains the package id and the second one contains
    the corresponding package object.
    """
    def __init__(self, transaction=None):
        gtk.ListStore.__init__(self,
                               gobject.TYPE_STRING,
                               gobject.TYPE_PYOBJECT)
        self._transaction = None
        self._signals = []
        if transaction != None:
            self.set_transaction(transaction)

    def set_transaction(self, transaction):
        """Connect the store to the given PackageKit transaction"""
        self.clear()
        for h in self._signals:
            h.remove()
        self._signals = []
        self._signals.append(
            transaction.connect_to_signal("Package", self._on_package))

    def _on_package(self, info, package_id, summary):
        self.append((package_id, PackageKitPackage(info, package_id, summary)))


class PackageListView(gtk.TreeView, gobject.GObject):
    """
    Provides a gtk.Listview which can show the packages of a PackageListStore
    """
    __gsignals__ = {"toggled":(gobject.SIGNAL_RUN_FIRST,
                               gobject.TYPE_NONE,
                               (gobject.TYPE_PYOBJECT,))}
    def __init__(self):
        self.__gobject_init__()
        gtk.TreeView.__init__(self)
        self.set_rules_hint(True)
        # Add a fake liststore to the packages list, so that the headers
        # are already seen during start up
        fake_store = gtk.ListStore(gobject.TYPE_STRING,
                                   gobject.TYPE_PYOBJECT)
        fake_store.set_sort_column_id(COLUMN_PACKAGE, gtk.SORT_ASCENDING)
        self.set_model(fake_store)

        self.theme = gtk.icon_theme_get_default()
        self.theme.append_search_path("/usr/share/gnome-packagekit/icons")

        # check boxes
        renderer_status = gtk.CellRendererToggle()
        renderer_status.connect('toggled', self._on_toggled)
        renderer_status.set_property("xalign", 0.5)
        renderer_status.set_property("yalign", 0.5)
        column_status = gtk.TreeViewColumn("")
        column_status.set_sizing(gtk.TREE_VIEW_COLUMN_FIXED)
        column_status.pack_start(renderer_status, False)
        column_status.set_cell_data_func (renderer_status, 
                                          self._toggle_cell_func)
        # FIXME: we need to react on theme changes
        width = renderer_status.get_size(self)[2] + 8
        column_status.set_fixed_width(width)

        # Application column (icon, name, description)
        column_pkg = gtk.TreeViewColumn(_("Package"))
        column_pkg.set_sizing(gtk.TREE_VIEW_COLUMN_FIXED)
        column_pkg.set_expand(True)
        column_pkg.set_sort_column_id(COLUMN_ID)
        # The icon
        renderer_icon = gtk.CellRendererPixbuf()
        column_pkg.pack_start(renderer_icon, False)
        column_pkg.set_cell_data_func(renderer_icon, 
                                      self._icon_cell_func)
        # package name and description
        renderer_desc = gtk.CellRendererText()
        renderer_desc.set_property("ellipsize", pango.ELLIPSIZE_END)
        column_pkg.pack_start(renderer_desc, True)
        column_pkg.set_cell_data_func(renderer_desc, 
                                      self._package_view_func)

        self.append_column(column_status)
        self.append_column(column_pkg)

        self.set_fixed_height_mode(True)

    def _package_view_func(self, cell_layout, renderer, model, iter):
        """
        Render the description of a package
        """
        pkg = model.get_value(iter, COLUMN_PACKAGE)
        text = "%s\n<small>%s</small>" % (pkg.name, pkg.summary)
        renderer.set_property("markup", text)

    def _toggle_cell_func(self, column, cell, model, iter):
        """
        Show a checked checkbox for installed package and disable the
        checkbox for blocked packages
        """
        pkg = model.get_value(iter, COLUMN_PACKAGE)
        cell.set_property("active", pkg.info == INFO_INSTALLED)
        cell.set_property("activatable", pkg.info != INFO_BLOCKED)

    def _icon_cell_func(self, column, cell, model, iter):
        """Render an icon that represents the info about of the package"""
        pkg = model.get_value(iter, COLUMN_PACKAGE)
        try:
            icon = self.theme.load_icon(get_info_icon_name_from_enum(pkg.info),
                                        24, 0)
            # work around bug LP #209072 even if we ask for a 24px
            # icon, we sometimes get outrages big ones - 256x256
            if icon and (icon.get_height() > 24 or icon.get_width() > 24):
                #print "WARNING: scaling down ", menuitem.iconname
                icon = icon.scale_simple(24, 24, gtk.gdk.INTERP_BILINEAR)
        except gobject.GError:
            try:
                icon = self.theme.load_icon("applications-other", 24, 0)
            except gobject.GError:
                icon = self.theme.load_icon(gtk.STOCK_MISSING_IMAGE, 24, 0)
        cell.set_property("pixbuf", icon)
        cell.set_property("visible", True)

    def _on_toggled(self, widget, path):
        """Emit the toggled signal with the selected package"""
        model = self.get_model()
        (id, pkg) = model[path]
        self.emit("toggled", pkg)


def main():
    """Run a test application"""
    import packagekit.client

    def on_exit(trans, exit, runtime):
        if exit == EXIT_FAILED:
            d = PackageKitMessageDialog(enum=trans._error_enum,
                                        details=trans._error_desc,
                                        parent=win)
            d.run()
            d.hide()
    def refresh(*args):
        t = pk.RefreshCache(exit_handler=on_exit)
        dia = PackageKitProgressDialog(t, parent=win)
        dia.run()
        dia.hide()
    def search(*args):
        t = pk.SearchName(FILTER_NONE, "xterm", exit_handler=on_exit)
        label.set_transaction(t)
        icon.set_transaction(t)
        pkgstore.set_transaction(t)
        t.run()
    def update(*args):
        t = pk.GetUpdates(FILTER_NONE, exit_handler=on_exit)
        label.set_transaction(t)
        icon.set_transaction(t)
        pkgstore.set_transaction(t)
        t.run()
    def on_toggled(view, pkg):
        if pkg.info == INFO_INSTALLED:
            t = pk.RemovePackages([pkg.id], False, False, exit_handler=on_exit)
        else:
            t = pk.InstallPackages([pkg.id], exit_handler=on_exit)
        dia = PackageKitProgressDialog(t, win)
        dia.run()
        dia.hide()
        search()
    win = gtk.Window()
    button_refresh = gtk.Button(label="Refresh")
    button_update = gtk.Button(label="Get Updates")
    button_search = gtk.Button(label="Search xterm")
    button_refresh.connect("clicked", refresh)
    button_update.connect("clicked", update)
    button_search.connect("clicked", search)
    vbox = gtk.VBox()
    vbox.add(button_update)
    vbox.add(button_refresh)
    vbox.add(button_search)
    pkgstore = PackageListStore()
    listview = PackageListView()
    listview.set_size_request(400, 200)
    scrolled = gtk.ScrolledWindow()
    scrolled.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
    scrolled.set_shadow_type(gtk.SHADOW_ETCHED_IN)
    scrolled.add(listview)
    vbox.add(scrolled)
    hbox = gtk.HBox()
    icon = PackageKitStatusIcon(size=gtk.ICON_SIZE_MENU)
    label = PackageKitStatusLabel()
    hbox.add(icon)
    hbox.add(label)
    vbox.add(hbox)
    win.add(vbox)
    loop = gobject.MainLoop()
    win.connect("delete-event", lambda w, e: loop.quit())
    win.show_all()
    listview.set_model(pkgstore)
    listview.connect("toggled", on_toggled)
    pk = packagekit.client.PackageKitClient()
    print map(lambda p: p.id, pk.Resolve(FILTER_NONE, "xterm"))
    loop.run()

if __name__ == "__main__":
    main()