#!/usr/bin/env python3
import sys
import os
import ftplib
import threading
import gi

gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, Gio, GObject, GLib
from astrodb import Library, Database, Table, ObjectSet, Search, AstroDBError, ADB_COMP_LT, ADB_COMP_GT, ADB_OP_AND, ADB_OP_OR

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
        conn_box.append(self.host_entry)

        self.remote_entry = Gtk.Entry()
        self.remote_entry.set_text("/pub/cats")
        self.remote_entry.set_placeholder_text("Remote Path")
        conn_box.append(self.remote_entry)

        self.local_entry = Gtk.Entry()
        self.local_entry.set_placeholder_text("Local Path")
        self.local_entry.set_hexpand(True)
        conn_box.append(self.local_entry)

        dir_btn = Gtk.Button(label="Select Dir")
        dir_btn.connect("clicked", self.on_select_dir)
        conn_box.append(dir_btn)

        self.connect_btn = Gtk.Button(label="Connect")
        self.connect_btn.connect("clicked", self.on_connect)
        conn_box.append(self.connect_btn)

        main_box.append(conn_box)

        self.status_label = Gtk.Label(label="Not connected")
        self.status_label.set_halign(Gtk.Align.START)
        main_box.append(self.status_label)

        main_box.append(Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL))

        # Bottom Area
        bottom_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
        bottom_box.set_vexpand(True)
        main_box.append(bottom_box)

        # 1. Left-most Vertical Toolbar
        toolbar_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
        Box_set_margin_all(toolbar_box, 4)
        
        self.local_toggle = Gtk.ToggleButton(label="Local")
        self.local_toggle.set_active(True)
        self.local_toggle.connect("toggled", self.on_catalog_toggle)
        toolbar_box.append(self.local_toggle)
        
        self.remote_toggle = Gtk.ToggleButton(label="Remote")
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
        
        catalog_label = Gtk.Label(label="Catalogs")
        catalog_label.set_margin_top(4)
        catalog_label.set_margin_bottom(4)
        catalog_box.append(catalog_label)

        # TreeStore columns: 0=Display Name, 1=Class, 2=ID, 3=Table Name, 4=Is Folder, 5=Path, 6=Is Loaded
        self.catalog_store = Gtk.TreeStore(str, str, str, str, bool, str, bool)
        self.catalog_treeview = Gtk.TreeView(model=self.catalog_store)
        
        renderer = Gtk.CellRendererText()
        col = Gtk.TreeViewColumn("Name", renderer, text=0)
        self.catalog_treeview.append_column(col)

        self.catalog_treeview.connect("row-activated", self.on_catalog_activated)
        self.catalog_treeview.connect("test-expand-row", self.on_catalog_expanding)
        
        sel = self.catalog_treeview.get_selection()
        sel.connect("changed", self.on_catalog_selected)

        scrolled_catalogs = Gtk.ScrolledWindow()
        scrolled_catalogs.set_child(self.catalog_treeview)
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
        scrolled_results.set_child(self.column_view)
        scrolled_results.set_vexpand(True)
        right_pane_box.append(scrolled_results)

        self.ftp_in_progress = False

        self.refresh_catalogs()

    def on_catalog_activated(self, treeview, path, column):
        if self.catalog_treeview.row_expanded(path):
            self.catalog_treeview.collapse_row(path)
        else:
            self.catalog_treeview.expand_row(path, False)

    def on_catalog_expanding(self, treeview, iter, path):
        # TreeStore columns: 0=Display Name, 1=Class, 2=ID, 3=Table Name, 4=Is Folder, 5=Path, 6=Is Loaded
        is_folder = self.catalog_store[iter][4]
        is_loaded = self.catalog_store[iter][6]
        remote_path = self.catalog_store[iter][5]

        if is_folder and not is_loaded and not self.local_toggle.get_active():
            # Loading from FTP
            if self.ftp_in_progress:
                return True # prevent expand while another is loading
            
            # Remove dummy child
            dummy_iter = self.catalog_store.iter_children(iter)
            if dummy_iter:
                self.catalog_store.remove(dummy_iter)

            self.catalog_store[iter][6] = True # Mark as loaded
            self.ftp_in_progress = True
            
            host = self.host_entry.get_text()
            
            # Spin up a thread to fetch FTP directory
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
                    
                    GLib.idle_add(self.on_ftp_fetched, path, entries)
                except Exception as e:
                    GLib.idle_add(self.on_ftp_error, path, str(e))

            thread = threading.Thread(target=fetch_ftp)
            thread.daemon = True
            thread.start()
            
            return True # Returning true prevents expansion until we manually expand it later

        return False

    def on_ftp_fetched(self, path, entries):
        self.ftp_in_progress = False
        iter = self.catalog_store.get_iter(path)
        parent_path = self.catalog_store[iter][5]
        
        for name, is_dir in sorted(entries, key=lambda x: (not x[1], x[0].lower())):
            full_path = f"{parent_path}/{name}"
            # Extract potential class and ID
            # This is very rough heuristics for CDS
            cat_cls, cat_id, tbl = "", "", ""
            p_parts = full_path.replace(self.remote_entry.get_text(), "").strip("/").split("/")
            
            if len(p_parts) == 1:
                cat_cls = p_parts[0]
            elif len(p_parts) == 2:
                cat_cls, cat_id = p_parts[0], p_parts[1]
            elif len(p_parts) >= 3:
                cat_cls, cat_id = p_parts[0], p_parts[1]
                tbl = name.replace(".db", "") if name.endswith(".db") else name

            child_iter = self.catalog_store.append(iter, [
                name, cat_cls, cat_id, tbl, is_dir, full_path, False
            ])
            
            if is_dir:
                # Append dummy
                self.catalog_store.append(child_iter, ["Loading...", "", "", "", False, "", True])
        
        self.catalog_treeview.expand_row(path, False)

    def on_ftp_error(self, path, err):
        self.ftp_in_progress = False
        iter = self.catalog_store.get_iter(path)
        self.catalog_store.append(iter, [f"Error: {err}", "", "", "", False, "", True])
        self.catalog_treeview.expand_row(path, False)

    def on_catalog_selected(self, selection):
        model, treeiter = selection.get_selected()
        if treeiter:
            name, cat_cls, cat_id, tbl_name, is_dir, pth, loaded = model[treeiter]
            if not is_dir and cat_cls and cat_id:
                self.selected_catalog = {
                    'name': name,
                    'cat_class': cat_cls,
                    'cat_id': cat_id,
                    'table_name': tbl_name
                }
                self.query_status.set_text(f"Selected table: {self.selected_catalog['name']}")
            else:
                self.selected_catalog = None
        else:
            self.selected_catalog = None

    def on_catalog_toggle(self, btn):
        if btn.get_active():
            self.refresh_catalogs()

    def refresh_catalogs(self):
        self.catalog_store.clear()
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
                        
                        # Add item to store directly
                        self.catalog_store.append(None, [
                            display_name, cat_class, cat_id, table_name, False, "", True
                        ])

    def populate_remote_catalogs(self):
        # We start by adding the root directory as the starting point.
        remote_path = self.remote_entry.get_text()
        parent_iter = self.catalog_store.append(None, [
            "CDS FTP Root", "", "", "", True, remote_path, False
        ])
        
        # Add dummy item to make it expandable
        self.catalog_store.append(parent_iter, [
            "Loading...", "", "", "", False, "", True
        ])

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
        except GLib.Error as e:
            print(f"Error selecting folder: {e}")

    def on_connect(self, btn):
        host = self.host_entry.get_text()
        remote = self.remote_entry.get_text()
        local = self.local_entry.get_text()

        if not local:
            self.status_label.set_text("Error: Local path required.")
            return

        try:
            self.lib = Library(host, remote, local)
            self.db = Database(self.lib, 9, 1)
            self.status_label.set_text(f"Connected to libadb version: {self.lib.version}")
            self.connect_btn.set_sensitive(False)
            self.local_entry.set_sensitive(False)
        except Exception as e:
            self.status_label.set_text(f"Connection failed: {e}")

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
