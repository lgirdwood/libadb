#!/usr/bin/env python3
import sys
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
        self.set_default_size(900, 600)
        self.set_title("AstroDB GTK4")

        self.lib = None
        self.db = None
        self.table = None

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

        # Directory Chooser Button setup
        dir_btn = Gtk.Button(label="Select Dir")
        dir_btn.connect("clicked", self.on_select_dir)
        conn_box.append(dir_btn)

        self.connect_btn = Gtk.Button(label="Connect")
        self.connect_btn.connect("clicked", self.on_connect)
        conn_box.append(self.connect_btn)

        main_box.append(conn_box)

        self.status_label = Gtk.Label(label="Not connected")
        main_box.append(self.status_label)

        # Separator
        main_box.append(Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL))

        # Two pane layout: Left Query, Right Results
        paned = Gtk.Paned(orientation=Gtk.Orientation.HORIZONTAL)
        paned.set_position(300)
        paned.set_vexpand(True)
        main_box.append(paned)

        # Left Query Pane
        query_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        Box_set_margin_all(query_box, 6)
        paned.set_start_child(query_box)

        # Table formulation
        table_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=6)
        table_box.append(Gtk.Label(label="Table:"))
        self.table_entry = Gtk.Entry()
        self.table_entry.set_text("gsc")
        self.table_entry.set_hexpand(True)
        table_box.append(self.table_entry)
        query_box.append(table_box)

        # Query Formulation
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

        self.query_status = Gtk.Label(label="")
        query_box.append(self.query_status)

        # Right Results Pane (ColumnView)
        self.list_store = Gio.ListStore(item_type=AstroObjectItem)
        self.selection_model = Gtk.SingleSelection(model=self.list_store)
        
        self.column_view = Gtk.ColumnView(model=self.selection_model)
        
        # Setup columns
        self.setup_column("ID", "id")
        self.setup_column("Designation", "designation")
        self.setup_column("RA", "ra", True)
        self.setup_column("DEC", "dec", True)
        self.setup_column("Mag", "mag", True)

        scrolled = Gtk.ScrolledWindow()
        scrolled.set_child(self.column_view)
        scrolled.set_vexpand(True)
        paned.set_end_child(scrolled)

    def setup_column(self, title, property_name, is_float=False):
        factory = Gtk.SignalListItemFactory()
        factory.connect("setup", self.on_list_item_setup, is_float)
        factory.connect("bind", self.on_list_item_bind, property_name, is_float)
        
        column = Gtk.ColumnViewColumn(title=title, factory=factory)
        self.column_view.append_column(column)

    def on_list_item_setup(self, factory, list_item, is_float):
        label = Gtk.Label()
        label.set_halign(Gtk.Align.START)
        list_item.set_child(label)

    def on_list_item_bind(self, factory, list_item, property_name, is_float):
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
        self.query_status.set_text("Searching...")
        self.list_store.remove_all()

        if not self.db:
            self.query_status.set_text("Not connected!")
            return

        try:
            table_name = self.table_entry.get_text()
            self.table = Table(self.db, "I", "254", table_name)
            
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
                # Try to clean up
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
