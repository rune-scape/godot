#include "register_types.h"

#include "core/class_db.h"
#include "editor/scene_tree_editor.h"

void register_scene_tree_dialog_types() {
    ClassDB::register_class<SceneTreeEditor>();
    ClassDB::register_class<SceneTreeDialog>();
    
    ClassDB::bind_method("get_scene_tree", &SceneTreeDialog::get_scene_tree);
    ClassDB::bind_method("set_valid_types", &SceneTreeEditor::set_valid_types);
}

void unregister_scene_tree_dialog_types() {
}
