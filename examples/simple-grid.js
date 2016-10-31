const Emeus = imports.gi.Emeus;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;

Emeus.ConstraintLayout.prototype.pack = function(child, name=null, constraints=[]) {
    let layout_child;

    if (child instanceof Emeus.ConstraintLayoutChild) {
        layout_child = child;
    }
    else {
        layout_child = new Emeus.ConstraintLayoutChild({ name: name });
        layout_child.add(child);
        layout_child.show();
    }

    this.add(layout_child);

    constraints.forEach(layout_child.add_constraint, layout_child);
};

Emeus.ConstraintLayout.prototype.add_constraints = function(constraints=[]) {
    constraints.forEach(this.add_constraint, this);
};

const MyApplicationWindow = new Lang.Class({
    Name: 'MyApplicationWindow',
    Extends: Gtk.ApplicationWindow,

    _init: function(props={}) {
        this.parent(props);
        this.set_default_size(400, 500);
        this.set_title('Grid layout');

        let box = new Gtk.Box({ orientation: Gtk.Orientation.VERTICAL, spacing: 6 });
        this.add(box);
        box.show();

        let layout = new Emeus.ConstraintLayout({ hexpand: true, vexpand: true });
        box.add(layout);
        layout.show();

        this._layout = layout;

        let button = new Gtk.Button({ label: 'Quit', hexpand: true });
        box.add(button);
        button.show();

        button.connect('clicked', this.destroy.bind(this));

        this._buildGrid();
    },

    _buildGrid: function() {
        /* Layout:
         *
         *   +-----------------------------+
         *   | +-----------+ +-----------+ |
         *   | |  Child 1  | |  Child 2  | |
         *   | +-----------+ +-----------+ |
         *   | +-------------------------+ |
         *   | |         Child 3         | |
         *   | +-------------------------+ |
         *   +-----------------------------+
         *
         * Visual format:
         *
         *   H:|-8-[child1(==child2)]-12-[child2]-8-|
         *   H:|-8-[child3]-8-|
         *   V:|-8-[child1]-12-[child3(==child1)]-8-|
         *   V:|-8-[child2]-12-[child3(==child2)]-8-|
         *
         * Constraints:
         *
         *   super.start = child1.start - 8
         *   child1.width = child2.width
         *   child1.end = child2.start - 12
         *   child2.end = super.end - 8
         *   super.start = child3.start - 8
         *   child3.end = super.end - 8
         *   super.top = child1.top - 8
         *   super.top = child2.top - 8
         *   child1.bottom = child3.top - 12
         *   child2.bottom = child3.top - 12
         *   child3.height = child1.height
         *   child3.height = child2.height
         *   child3.bottom = super.bottom - 8
         *
         */
        let button1 = new Gtk.Button({ label: 'Child 1' });
        this._layout.pack(button1, 'child1');
        button1.show();

        let button2 = new Gtk.Button({ label: 'Child 2' });
        this._layout.pack(button2, 'child2');
        button2.show();

        let button3 = new Gtk.Button({ label: 'Child 3' });
        this._layout.pack(button3, 'child3');
        button3.show();

        let constraints = Emeus.create_constraints_from_description([
                'H:|-8-[child1(==child2)]-12-[child2]-8-|',
                'H:|-8-[child3]-8-|',
                'V:|-8-[child1]-12-[child3(==child1)]-8-|',
                'V:|-8-[child2]-12-[child3(==child2)]-8-|',
            ], 8, 8, {
                child1: button1,
                child2: button2,
                child3: button3,
            }, { });

        this._layout.add_constraints(constraints);
    },
});

const MyApplication = new Lang.Class({
    Name: 'MyApplication',
    Extends: Gtk.Application,

    _init: function() {
        this.parent({ application_id: 'io.bassi.SimpleGrid' });

        this._mainWindow = null;
    },

    vfunc_activate: function() {
        if (!this._mainWindow) {
            this._mainWindow = new MyApplicationWindow({ application: this });
            this._mainWindow.show();
        }

        this._mainWindow.present();
    },
});

let app = new MyApplication();
app.run(['simple-grid'].concat(ARGV));
