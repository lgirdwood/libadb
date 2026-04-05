#!/usr/bin/env python3
import sys
import os
import ftplib
import threading
import gi

gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, Gio, GObject, GLib, Pango
from astrodb import Library, Database, Table, ObjectSet, Search, AstroDBError, ADB_COMP_LT, ADB_COMP_GT, ADB_OP_AND, ADB_OP_OR

class CatalogItem(GObject.Object):
    __gtype_name__ = 'CatalogItem'
    
    display_name = GObject.Property(type=str)
    cat_class = GObject.Property(type=str)
    cat_id = GObject.Property(type=str)
    table_name = GObject.Property(type=str)
    is_folder = GObject.Property(type=bool, default=False)
    path = GObject.Property(type=str)
    is_loaded = GObject.Property(type=bool, default=False)
    children = GObject.Property(type=Gio.ListStore)

    def __init__(self, display_name, cat_class, cat_id, table_name, is_folder, path, is_loaded):
        super().__init__()
        self.display_name = display_name
        self.cat_class = cat_class
        self.cat_id = cat_id
        self.table_name = table_name
        self.is_folder = is_folder
        self.path = path
        self.is_loaded = is_loaded
        if is_folder:
            self.children = Gio.ListStore(item_type=CatalogItem)
        else:
            self.children = None

class AstroObjectItem(GObject.Object):
    __gtype_name__ = 'AstroObjectItem'
    
    id = GObject.Property(type=int)
    designation = GObject.Property(type=str)
    ra = GObject.Property(type=float)
    dec = GObject.Property(type=float)
    mag = GObject.Property(type=float)

    def __init__(self, id_val, designation, ra, dec, mag):
        super().__init__()
        self.id = id_val
        self.designation = designation
        self.ra = ra
        self.dec = dec
        self.mag = mag

def Box_set_margin_all(box, margin):
    box.set_margin_top(margin)
    box.set_margin_bottom(margin)
    box.set_margin_start(margin)
    box.set_margin_end(margin)

class AstroDBWindow(Gtk.ApplicationWindow):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.set_default_size(1050, 600)
        self.set_title("AstroDB GTK4")

        self.lib = None
        self.db = None
        self.table = None
        self.selected_catalog = None

        main_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        Box_set_margin_all(main_box, 10)
        self.set_child(main_box)

        # Connection Header
        conn_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=6)
        
        self.host_entry = Gtk.Entry()
        self.host_entry.set_text("cdsarc.u-strasbg.fr")
        self.host_entry.set_placeholder_text("Host")

        self.remote_entry = Gtk.Entry()
        self.remote_entry.set_text("/pub/cats")
        self.remote_entry.set_placeholder_text("Remote Path")

        self.local_entry = Gtk.Entry()
        self.local_entry.set_placeholder_text("Local Path")
        self.local_entry.set_hexpand(True)

        # Connection status icon moved to titlebar
        # Bottom Area
        bottom_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
        bottom_box.set_vexpand(True)
        main_box.append(bottom_box)

        # 1. Left-most Vertical Toolbar
        toolbar_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
        Box_set_margin_all(toolbar_box, 4)
        
        self.local_toggle = Gtk.ToggleButton()
        self.local_toggle.set_icon_name("folder-symbolic")
        self.local_toggle.set_tooltip_text("Local Catalogs")
        self.local_toggle.set_active(True)
        self.local_toggle.connect("toggled", self.on_catalog_toggle)
        toolbar_box.append(self.local_toggle)
        
        self.remote_toggle = Gtk.ToggleButton()
        self.remote_toggle.set_icon_name("network-server-symbolic")
        self.remote_toggle.set_tooltip_text("Remote Catalogs")
        self.remote_toggle.connect("toggled", self.on_catalog_toggle)
        toolbar_box.append(self.remote_toggle)

        self.remote_toggle.set_group(self.local_toggle)
        
        bottom_box.append(toolbar_box)
        bottom_box.append(Gtk.Separator(orientation=Gtk.Orientation.VERTICAL))

        # 2. Paned area
        paned = Gtk.Paned(orientation=Gtk.Orientation.HORIZONTAL)
        paned.set_position(300)
        paned.set_hexpand(True)
        bottom_box.append(paned)

        # Left Pane: Catalog TreeStore (better for hierarchies than ListStore in pure python GTK4)
        catalog_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
        
        header_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        header_box.set_margin_top(4)
        header_box.set_margin_bottom(4)
        
        catalog_label = Gtk.Label()
        catalog_label.set_markup("<span size='small' weight='bold'>Explorer</span>")
        catalog_label.set_halign(Gtk.Align.START)
        catalog_label.set_hexpand(True)
        header_box.append(catalog_label)
        
        options_btn = Gtk.Button(label="...")
        options_btn.add_css_class("flat")
        options_btn.connect("clicked", self.on_explorer_options)
        header_box.append(options_btn)
        
        catalog_box.append(header_box)

        self.catalog_store = Gio.ListStore(item_type=CatalogItem)
        
        def tree_create_model(item, user_data=None):
            if item.is_folder:
                return item.children
            return None
            
        self.tree_model = Gtk.TreeListModel.new(self.catalog_store, passthrough=False, autoexpand=False, create_func=tree_create_model)
        
        self.catalog_selection = Gtk.SingleSelection(model=self.tree_model)
        self.catalog_selection.connect("selection-changed", self.on_catalog_selected)
        
        self.catalog_listview = Gtk.ListView(model=self.catalog_selection)
        
        factory = Gtk.SignalListItemFactory()
        factory.connect("setup", self.on_catalog_item_setup)
        factory.connect("bind", self.on_catalog_item_bind)
        
        self.catalog_listview.set_factory(factory)

        self.list_header_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=6)
        self.list_header_box.set_margin_top(4)
        self.list_header_box.set_margin_bottom(4)
        self.list_header_box.set_margin_start(4)
        
        self.status_icon = Gtk.Image.new_from_icon_name("network-offline-symbolic")
        self.status_icon.set_tooltip_text("Not connected")
        self.list_header_box.append(self.status_icon)
        
        self.url_label = Gtk.Label()
        self.url_label.set_halign(Gtk.Align.START)
        self.url_label.set_hexpand(True)
        self.url_label.set_ellipsize(Pango.EllipsizeMode.END)
        self.list_header_box.append(self.url_label)
        self.update_catalog_title()
        
        catalog_box.append(self.list_header_box)
        catalog_box.append(Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL))

        scrolled_catalogs = Gtk.ScrolledWindow()
        scrolled_catalogs.set_min_content_width(200)
        scrolled_catalogs.set_min_content_height(200)
        scrolled_catalogs.set_child(self.catalog_listview)
        scrolled_catalogs.set_vexpand(True)
        catalog_box.append(scrolled_catalogs)
        
        paned.set_start_child(catalog_box)

        # Right Pane: Queries and Results
        right_pane_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        Box_set_margin_all(right_pane_box, 6)
        paned.set_end_child(right_pane_box)

        query_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=6)
        
        self.ra_min_entry = Gtk.Entry(placeholder_text="RA Min (deg)")
        query_box.append(self.ra_min_entry)
        
        self.ra_max_entry = Gtk.Entry(placeholder_text="RA Max (deg)")
        query_box.append(self.ra_max_entry)

        self.dec_min_entry = Gtk.Entry(placeholder_text="DEC Min (deg)")
        query_box.append(self.dec_min_entry)

        self.dec_max_entry = Gtk.Entry(placeholder_text="DEC Max (deg)")
        query_box.append(self.dec_max_entry)

        self.search_btn = Gtk.Button(label="Search")
        self.search_btn.connect("clicked", self.on_search)
        query_box.append(self.search_btn)

        right_pane_box.append(query_box)

        self.query_status = Gtk.Label(label="")
        self.query_status.set_halign(Gtk.Align.START)
        right_pane_box.append(self.query_status)

        # Right Results Pane (ColumnView)
        self.list_store = Gio.ListStore(item_type=AstroObjectItem)
        self.selection_model = Gtk.SingleSelection(model=self.list_store)
        
        self.column_view = Gtk.ColumnView(model=self.selection_model)
        
        self.setup_result_column("ID", "id")
        self.setup_result_column("Designation", "designation")
        self.setup_result_column("RA", "ra", True)
        self.setup_result_column("DEC", "dec", True)
        self.setup_result_column("Mag", "mag", True)

        scrolled_results = Gtk.ScrolledWindow()
        scrolled_results.set_min_content_width(400)
        scrolled_results.set_min_content_height(200)
        scrolled_results.set_child(self.column_view)
        scrolled_results.set_vexpand(True)
        right_pane_box.append(scrolled_results)

        self.ftp_in_progress = False

        self.refresh_catalogs()

    def on_catalog_item_setup(self, factory, list_item):
        expander = Gtk.TreeExpander()
        label = Gtk.Label()
        label.set_halign(Gtk.Align.START)
        expander.set_child(label)
        list_item.set_child(expander)

    def on_catalog_item_bind(self, factory, list_item):
        expander = list_item.get_child()
        tree_row = list_item.get_item()
        catalog_item = tree_row.get_item()
        
        expander.set_list_row(tree_row)
        label = expander.get_child()
        label.set_text(catalog_item.display_name)
        
        if hasattr(list_item, '_expanded_handler_id'):
            tree_row.disconnect(list_item._expanded_handler_id)
            delattr(list_item, '_expanded_handler_id')
            
        list_item._expanded_handler_id = tree_row.connect("notify::expanded", self.on_catalog_expanded)

    def on_catalog_expanded(self, tree_row, param):
        if tree_row.get_expanded():
            catalog_item = tree_row.get_item()
            if catalog_item.is_folder and not catalog_item.is_loaded and not self.local_toggle.get_active():
                if self.ftp_in_progress:
                    return
                catalog_item.is_loaded = True
                self.ftp_in_progress = True
                
                host = self.host_entry.get_text()
                remote_path = catalog_item.path
                
                def fetch_ftp():
                    try:
                        ftp = ftplib.FTP(host)
                        ftp.login()
                        ftp.cwd(remote_path)
                        
                        lines = []
                        ftp.dir(lines.append)
                        ftp.quit()
                        
                        entries = []
                        for line in lines:
                            parts = line.split()
                            if len(parts) < 9: continue
                            name = parts[-1]
                            is_dir = line.startswith('d')
                            if name in ('.', '..'): continue
                            entries.append((name, is_dir))
                        
                        GLib.idle_add(self.on_ftp_fetched, catalog_item, entries)
                    except Exception as e:
                        GLib.idle_add(self.on_ftp_error, catalog_item, str(e))

                thread = threading.Thread(target=fetch_ftp)
                thread.daemon = True
                thread.start()

    def on_ftp_fetched(self, catalog_item, entries):
        self.ftp_in_progress = False
        parent_path = catalog_item.path
        
        for name, is_dir in sorted(entries, key=lambda x: (not x[1], x[0].lower())):
            full_path = f"{parent_path}/{name}"
            cat_cls, cat_id, tbl = "", "", ""
            p_parts = full_path.replace(self.remote_entry.get_text(), "").strip("/").split("/")
            
            if len(p_parts) == 1:
                cat_cls = p_parts[0]
            elif len(p_parts) == 2:
                cat_cls, cat_id = p_parts[0], p_parts[1]
            elif len(p_parts) >= 3:
                cat_cls, cat_id = p_parts[0], p_parts[1]
                tbl = name.replace(".db", "") if name.endswith(".db") else name

            item = CatalogItem(name, cat_cls, cat_id, tbl, is_dir, full_path, False)
            catalog_item.children.append(item)

    def on_ftp_error(self, catalog_item, err):
        self.ftp_in_progress = False
        item = CatalogItem(f"Error: {err}", "", "", "", False, "", True)
        catalog_item.children.append(item)

    def on_catalog_selected(self, selection, position, n_items):
        selected_item = self.catalog_selection.get_selected_item()
        if selected_item:
            catalog_item = selected_item.get_item()
            if not catalog_item.is_folder and catalog_item.cat_class and catalog_item.cat_id:
                self.selected_catalog = {
                    'name': catalog_item.display_name,
                    'cat_class': catalog_item.cat_class,
                    'cat_id': catalog_item.cat_id,
                    'table_name': catalog_item.table_name
                }
                self.query_status.set_text(f"Selected table: {self.selected_catalog['name']}")
                return
        self.selected_catalog = None

    def on_explorer_options(self, btn):
        if self.local_toggle.get_active():
            self.on_select_dir(btn)
        else:
            popover = Gtk.Popover()
            popover.set_parent(btn)
            
            box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
            box.set_margin_top(10)
            box.set_margin_bottom(10)
            box.set_margin_start(10)
            box.set_margin_end(10)
            
            h_lbl = Gtk.Label(label="Host")
            h_lbl.set_halign(Gtk.Align.START)
            box.append(h_lbl)
            
            h_entry = Gtk.Entry()
            h_entry.set_text(self.host_entry.get_text())
            box.append(h_entry)
            
            p_lbl = Gtk.Label(label="Path")
            p_lbl.set_halign(Gtk.Align.START)
            box.append(p_lbl)
            
            p_entry = Gtk.Entry()
            p_entry.set_text(self.remote_entry.get_text())
            box.append(p_entry)
            
            self.connect_btn = Gtk.Button(label="Connect DB")
            self.connect_btn.add_css_class("suggested-action")
                
            def on_apply(b):
                self.host_entry.set_text(h_entry.get_text())
                self.remote_entry.set_text(p_entry.get_text())
                self.refresh_catalogs()
                self.on_connect(b)
                popover.unparent()
            
            self.connect_btn.connect("clicked", on_apply)
            box.append(self.connect_btn)
            
            popover.set_child(box)
            popover.popup()

    def on_catalog_toggle(self, btn):
        if btn.get_active():
            self.refresh_catalogs()

    def update_catalog_title(self):
        if hasattr(self, 'url_label'):
            if self.local_toggle.get_active():
                path = self.local_entry.get_text()
                if not path:
                    path = "Local Database"
                self.url_label.set_markup(f"<span size='small' weight='bold'>{path}</span>")
            else:
                path = self.host_entry.get_text() + self.remote_entry.get_text()
                self.url_label.set_markup(f"<span size='small' weight='bold'>{path}</span>")

    def refresh_catalogs(self):
        self.update_catalog_title()
        self.catalog_store.remove_all()
        if self.local_toggle.get_active():
            self.populate_local_catalogs()
        else:
            self.populate_remote_catalogs()

    def populate_local_catalogs(self):
        local_dir = self.local_entry.get_text()
        if not local_dir or not os.path.exists(local_dir):
            return

        for root, dirs, files in os.walk(local_dir):
            for file in files:
                if file.endswith(".db"):
                    rel_path = os.path.relpath(root, local_dir)
                    parts = rel_path.split(os.sep)
                    if len(parts) >= 2:
                        cat_class = parts[-2]
                        cat_id = parts[-1]
                        table_name = file[:-3]
                        display_name = f"{cat_class}/{cat_id} - {table_name}"
                        
                        item = CatalogItem(display_name, cat_class, cat_id, table_name, False, "", True)
                        self.catalog_store.append(item)

    def populate_remote_catalogs(self):
        remote_path = self.remote_entry.get_text()
        item = CatalogItem("CDS FTP Root", "", "", "", True, remote_path, False)
        self.catalog_store.append(item)

    def setup_result_column(self, title, property_name, is_float=False):
        factory = Gtk.SignalListItemFactory()
        factory.connect("setup", self.on_result_item_setup, is_float)
        factory.connect("bind", self.on_result_item_bind, property_name, is_float)
        
        column = Gtk.ColumnViewColumn(title=title, factory=factory)
        self.column_view.append_column(column)

    def on_result_item_setup(self, factory, list_item, is_float):
        label = Gtk.Label()
        label.set_halign(Gtk.Align.START)
        list_item.set_child(label)

    def on_result_item_bind(self, factory, list_item, property_name, is_float):
        label = list_item.get_child()
        item = list_item.get_item()
        val = getattr(item, property_name)
        if isinstance(val, (bytes, bytearray)):
            val = val.decode('utf-8', 'ignore')
        if is_float and val is not None:
            label.set_text(f"{val:.6f}")
        else:
            label.set_text(str(val))

    def on_select_dir(self, btn):
        dialog = Gtk.FileDialog()
        dialog.set_title("Select Local Database Directory")
        dialog.select_folder(self, None, self.on_folder_selected)

    def on_folder_selected(self, dialog, result):
        try:
            folder = dialog.select_folder_finish(result)
            if folder:
                self.local_entry.set_text(folder.get_path())
                if self.local_toggle.get_active():
                    self.refresh_catalogs()
                self.on_connect(None)
        except GLib.Error as e:
            print(f"Error selecting folder: {e}")

    def on_connect(self, btn):
        host = self.host_entry.get_text()
        remote = self.remote_entry.get_text()
        local = self.local_entry.get_text()

        if not local:
            self.status_icon.set_from_icon_name("dialog-error-symbolic")
            self.status_icon.set_tooltip_text("Error: Local path required.")
            return

        try:
            self.lib = Library(host, remote, local)
            self.db = Database(self.lib, 9, 1)
            self.status_icon.set_from_icon_name("network-server-symbolic")
            self.status_icon.set_tooltip_text(f"Connected to libadb version: {self.lib.version}")
            self.local_entry.set_sensitive(False)
        except Exception as e:
            self.status_icon.set_from_icon_name("dialog-error-symbolic")
            self.status_icon.set_tooltip_text(f"Connection failed: {e}")

    def on_search(self, btn):
        self.list_store.remove_all()

        if not self.db:
            self.query_status.set_text("Not connected!")
            return

        if not self.selected_catalog:
            self.query_status.set_text("Please select a catalog first!")
            return

        self.query_status.set_text(f"Searching {self.selected_catalog['name']}...")

        try:
            cat_cls = self.selected_catalog['cat_class']
            cat_id = self.selected_catalog['cat_id']
            table_name = self.selected_catalog['table_name']
            self.table = Table(self.db, cat_cls, cat_id, table_name)
            
            ra_min = self.ra_min_entry.get_text()
            ra_max = self.ra_max_entry.get_text()
            dec_min = self.dec_min_entry.get_text()
            dec_max = self.dec_max_entry.get_text()
            
            search = Search(self.table)
            oset = ObjectSet(self.table)

            has_constraints = False
            
            if ra_min:
                search.add_comparator("RAdeg", ADB_COMP_GT, ra_min)
                has_constraints = True
            if ra_max:
                search.add_comparator("RAdeg", ADB_COMP_LT, ra_max)
                if has_constraints: search.add_operator(ADB_OP_AND)
                has_constraints = True
            
            if dec_min:
                search.add_comparator("DEdeg", ADB_COMP_GT, dec_min)
                if has_constraints: search.add_operator(ADB_OP_AND)
                has_constraints = True
            if dec_max:
                search.add_comparator("DEdeg", ADB_COMP_LT, dec_max)
                if has_constraints: search.add_operator(ADB_OP_AND)
                has_constraints = True

            if has_constraints:
                hits = search.execute(oset)
                self.query_status.set_text(f"Found {hits} objects. Generating results...")
                
                R2D = 57.295779513082320877
                
                for obj in search:
                    desig = obj.designation
                    if isinstance(desig, bytes):
                        desig = desig.decode('utf-8', 'ignore')
                    
                    item = AstroObjectItem(
                        obj.id, 
                        desig,
                        obj.ra * R2D,
                        obj.dec * R2D,
                        obj.mag
                    )
                    self.list_store.append(item)
            else:
                self.query_status.set_text("Please provide at least one search constraint.")

            search.close()
            oset.close()
            self.table.close()
            self.table = None
            self.query_status.set_text(f"Search complete. {self.list_store.get_n_items()} items found.")
            
        except Exception as e:
            self.query_status.set_text(f"Search error: {e}")
            if self.table:
                self.table.close()
                self.table = None

class AstroDBApp(Gtk.Application):
    def __init__(self):
        super().__init__(application_id="org.libadb.AstroDBViewer")

    def do_activate(self):
        win = getattr(self.props, 'active_window', None)
        if not win:
            win = AstroDBWindow(application=self)
        win.present()

if __name__ == "__main__":
    app = AstroDBApp()
    sys.exit(app.run(sys.argv))
