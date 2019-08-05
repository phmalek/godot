#include "animation_motion_match_editor.h"
#include <limits>
#include <iostream>

#ifdef TOOLS_ENABLED
#include "core/io/resource_loader.h"

bool AnimationNodeMotionMatchEditor::can_edit(
    const Ref<AnimationNode> &p_node) {

  Ref<AnimationNodeMotionMatch> anmm = p_node;
  return anmm.is_valid();
}

void AnimationNodeMotionMatchEditor::edit(const Ref<AnimationNode> &p_node) {

  motion_match = p_node;

  if (motion_match.is_valid()) {
  }
}

void AnimationNodeMotionMatchEditor::_match_tracks_edited() {
  if (updating) {
    return;
  }

  TreeItem *edited = match_tracks->get_edited();
  ERR_FAIL_COND(!edited);

  NodePath edited_path = edited->get_metadata(0);
  bool matched = edited->is_checked(0);

  UndoRedo *undo_redo = EditorNode::get_singleton()->get_undo_redo();

  updating = true;
  undo_redo->create_action(TTR("Change Match Track"));
  if (matched) {
    undo_redo->add_do_method(motion_match.ptr(), "add_matching_track",
                             edited_path);
    undo_redo->add_undo_method(motion_match.ptr(), "remove_matching_track",
                               edited_path);
  } else {
    undo_redo->add_do_method(motion_match.ptr(), "remove_matching_track",
                             edited_path);
    undo_redo->add_undo_method(motion_match.ptr(), "add_matching_track",
                               edited_path);
  }
  undo_redo->add_do_method(this, "_update_match_tracks");
  undo_redo->add_undo_method(this, "_update_match_tracks");
  undo_redo->commit_action();
  updating = false;
}

void AnimationNodeMotionMatchEditor::_edit_match_tracks() {

  _update_match_tracks();
  match_tracks_dialog->popup_centered_minsize(Size2(500, 500) * EDSCALE);
}

void AnimationNodeMotionMatchEditor::_update_match_tracks() {

  if (!is_visible()) {
    return;
  }

  if (updating) {
    return;
  }

  /*Checking for errors*/

  NodePath player_path =
      AnimationTreeEditor::get_singleton()->get_tree()->get_animation_player();
  if (!AnimationTreeEditor::get_singleton()->get_tree()->has_node(
          player_path)) {
    EditorNode::get_singleton()->show_warning(
        TTR("No animation player set, so unable to retrieve track names."));
    return;
  }

  AnimationPlayer *player = Object::cast_to<AnimationPlayer>(
      AnimationTreeEditor::get_singleton()->get_tree()->get_node(player_path));
  if (!player) {
    EditorNode::get_singleton()->show_warning(
        TTR("Player path set is invalid, so unable to retrieve track names."));
    return;
  }

  Node *base = player->get_node(player->get_root());
  if (!base) {
    EditorNode::get_singleton()->show_warning(
        TTR("Animation player has no valid root node path, so unable to "
            "retrieve track names."));
    return;
  }
  /**/

  /*Get the list of all bones and display it*/
  updating = true;

  Set<String> paths;
  List<StringName> animations;
  player->get_animation_list(&animations);

  for (List<StringName>::Element *E = animations.front(); E; E = E->next()) {

    Ref<Animation> anim = player->get_animation(E->get());
    for (int i = 0; i < anim->get_track_count(); i++) {
      paths.insert(anim->track_get_path(i));
    }
  }

  match_tracks->clear();
  TreeItem *root = match_tracks->create_item();

  Map<String, TreeItem *> parenthood;

  for (Set<String>::Element *E = paths.front(); E; E = E->next()) {
    NodePath path = E->get();
    TreeItem *ti = NULL;
    String accum;
    for (int i = 0; i < path.get_name_count(); i++) {
      String name = path.get_name(i);
      if (accum != String()) {
        accum += "/";
      }
      accum += name;
      if (!parenthood.has(accum)) {
        if (ti) {
          ti = match_tracks->create_item(ti);
        } else {
          ti = match_tracks->create_item(root);
        }
        parenthood[accum] = ti;
        ti->set_text(0, name);
        ti->set_selectable(0, false);
        ti->set_editable(0, false);
        if (base->has_node(accum)) {
          Node *node = base->get_node(accum);
          ti->set_icon(
              0, EditorNode::get_singleton()->get_object_icon(node, "Node"));
        }

      } else {
        ti = parenthood[accum];
      }
    }
    Node *node = NULL;
    if (base->has_node(accum)) {
      node = base->get_node(accum);
    }
    if (!node)
      continue; // no node, can't edit

    if (path.get_subname_count()) {

      String concat = path.get_concatenated_subnames();
      this->skeleton = Object::cast_to<Skeleton>(node);
      if (skeleton && skeleton->find_bone(concat) != -1) {
        // path in skeleton
        String bone = concat;
        int idx = skeleton->find_bone(bone);
        List<String> bone_path;
        while (idx != -1) {
          bone_path.push_front(skeleton->get_bone_name(idx));
          idx = skeleton->get_bone_parent(idx);
        }

        accum += ":";
        for (List<String>::Element *F = bone_path.front(); F; F = F->next()) {
          if (F != bone_path.front()) {
            accum += "/";
          }

          accum += F->get();
          if (!parenthood.has(accum)) {
            ti = match_tracks->create_item(ti);
            parenthood[accum] = ti;
            ti->set_text(0, F->get());
            ti->set_selectable(0, false);
            ti->set_editable(0, false);
            ti->set_icon(0, get_icon("BoneAttachment", "EditorIcons"));
          } else {
            ti = parenthood[accum];
          }
        }

        ti->set_editable(0, true);
        ti->set_selectable(0, true);
        ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
        ti->set_text(0, concat);
        ti->set_checked(0, motion_match->is_matching_track(path));
        ti->set_icon(0, get_icon("BoneAttachment", "EditorIcons"));
        ti->set_metadata(0, path);

      } else {
        // just a property
        ti = match_tracks->create_item(ti);
        ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
        ti->set_text(0, concat);
        ti->set_editable(0, true);
        ti->set_selectable(0, true);
        ti->set_checked(0, motion_match->is_matching_track(path));
        ti->set_metadata(0, path);
      }
    } else {
      if (ti) {
        // just a node, likely call or animation track
        ti->set_editable(0, true);
        ti->set_selectable(0, true);
        ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
        ti->set_checked(0, motion_match->is_matching_track(path));
        ti->set_metadata(0, path);
      }
    }
  }
  /**/
  updating = false;
}

void AnimationNodeMotionMatchEditor::_update_tracks() {

  NodePath player_path =
      AnimationTreeEditor::get_singleton()->get_tree()->get_animation_player();
  /*Checking for errors*/
  if (!AnimationTreeEditor::get_singleton()->get_tree()->has_node(
          player_path)) {
    EditorNode::get_singleton()->show_warning(
        TTR("No animation player set, so unable to retrieve track names."));
    return;
  }

  AnimationPlayer *player = Object::cast_to<AnimationPlayer>(
      AnimationTreeEditor::get_singleton()->get_tree()->get_node(player_path));
  if (!player) {
    EditorNode::get_singleton()->show_warning(
        TTR("Player path set is invalid, so unable to retrieve track names."));
    return;
  }

  if ((AnimationTreeEditor::get_singleton()
           ->get_tree()
           ->get_root_motion_track() == NodePath())) {
    EditorNode::get_singleton()->show_warning(
        TTR("No root motion track was set, unable to build database."));
    return;
  }

  /**/
  /*UPDATING DATABASE*/
  motion_match->set_dim_len(9);
  List<StringName> Animations;
  player->get_animation_list(&Animations);

  for (int i = 0; i < Animations.size(); i++) {
    Ref<Animation> anim = player->get_animation(Animations[i]);
    int root = anim->find_track(AnimationTreeEditor::get_singleton()
                                    ->get_tree()
                                    ->get_root_motion_track());
    int max_count =
        fill_tracks(player, anim.ptr(), AnimationTreeEditor::get_singleton()
                                            ->get_tree()
                                            ->get_root_motion_track());

    for (int j = 0; j < max_count - samples; j++) {
      int quad = 0;
      float x = 0;
      float z = 0;
      for (int p = 0; p < 2; p++) {
        x += Math::pow(-1.0, double(p + 1)) *
             Vector3(Dictionary(anim->track_get_key_value(root, j + p))
                         .get("location", Variant()))[0];
        z += Math::pow(-1.0, double(p + 1)) *
             Vector3(Dictionary(anim->track_get_key_value(root, j + p))
                         .get("location", Variant()))[2];
      }

      if (x > 0 && z > 0) {
        quad = 1;
      } else if (x < 0 && z > 0) {
        quad = 2;
      } else if (x < 0 && z < 0) {
        quad = 3;
      } else {
        quad = 4;
      }
      frame_model *key = new frame_model;
      for (int y = 0; y < motion_match->get_matching_tracks().size(); y++) {
        int track = anim->find_track(motion_match->get_matching_tracks()[y]);
        Vector3 loc = Vector3(Dictionary(anim->track_get_key_value(track, j))
                                  .get("location", Variant()));
        PoolRealArray arr = {};
        for (int l = 0; l < 3; l++) {
          if (l != 1) {
            arr.append(loc[l]);
          }
        }
        key->bone_data->append(arr);
      }
      Vector3 r_loc = Vector3(Dictionary(anim->track_get_key_value(root, j))
                                  .get("location", Variant()));
      for (int k = 0; k < samples; k++) {
        Vector3 loc = Vector3(Dictionary(anim->track_get_key_value(root, j + k))
                                  .get("location", Variant()));
        for (int l = 0; l < 3; l++) {
          if (l != 1) {
            key->traj->append(loc[l] - r_loc[l]);
          }
        }
      }

      if (quad == 2) {
        for (int m = 0; m < key->traj->size(); m += 2) {
          float t = key->traj->read()[m];
          key->traj->write()[m] = -key->traj->read()[m + 1];
          key->traj->write()[m + 1] = t;
        }
      } else if (quad == 3) {
        for (int m = 0; m < key->traj->size(); m += 2) {
          key->traj->write()[m] = -key->traj->read()[m];
          key->traj->write()[m + 1] = -key->traj->read()[m + 1];
        }
      } else if (quad == 4) {
        for (int m = 0; m < key->traj->size(); m += 2) {
          float t = key->traj->read()[m];
          key->traj->write()[m] = key->traj->read()[m + 1];
          key->traj->write()[m + 1] = -t;
        }
      }

      key->time = anim->track_get_key_time(root, j);
      key->anim_num = i;
      keys->append(key);
    }

    motion_match->set_delta_time(anim->track_get_key_time(root, 0));
  }
  motion_match->clear_keys();
  motion_match->set_keys_data(keys);
  motion_match->skeleton = skeleton;
  print_line("DONE");
  /**/
}

void AnimationNodeMotionMatchEditor::_bind_methods() {
  ClassDB::bind_method("_match_tracks_edited",
                       &AnimationNodeMotionMatchEditor::_match_tracks_edited);
  ClassDB::bind_method("_update_match_tracks",
                       &AnimationNodeMotionMatchEditor::_update_match_tracks);
  ClassDB::bind_method("_edit_match_tracks",
                       &AnimationNodeMotionMatchEditor::_edit_match_tracks);
  ClassDB::bind_method("_update_tracks",
                       &AnimationNodeMotionMatchEditor::_update_tracks);
}
AnimationNodeMotionMatchEditor::AnimationNodeMotionMatchEditor() {

  match_tracks_dialog = memnew(AcceptDialog);
  add_child(match_tracks_dialog);
  match_tracks_dialog->set_title(TTR("Tracks to Match:"));

  VBoxContainer *match_tracks_vbox = memnew(VBoxContainer);
  match_tracks_dialog->add_child(match_tracks_vbox);

  match_tracks = memnew(Tree);
  match_tracks_vbox->add_child(match_tracks);
  match_tracks->set_v_size_flags(SIZE_EXPAND_FILL);
  match_tracks->set_hide_root(true);
  match_tracks->connect("item_edited", this, "_match_tracks_edited");

  edit_match_tracks = memnew(Button("Edit Tracks "));
  add_child(edit_match_tracks);
  edit_match_tracks->connect("pressed", this, "_edit_match_tracks");

  update_tracks = memnew(Button("Update Tree"));
  add_child(update_tracks);
  update_tracks->connect("pressed", this, "_update_tracks");

  updating = false;
}

int AnimationNodeMotionMatchEditor::fill_tracks(AnimationPlayer *player,
                                                Animation *anim,
                                                NodePath &root) {
  int max_keys = 0;
  Vector<NodePath> tracks_tf = motion_match->get_matching_tracks();
  tracks_tf.push_back(root);
  for (int i = 0; i < tracks_tf.size(); i++) {
    if (anim->track_get_key_count(anim->find_track(tracks_tf[i])) >
        anim->track_get_key_count(anim->find_track(tracks_tf[max_keys]))) {
      max_keys = i;
    }
  }
  for (int i = 0;
       i < anim->track_get_key_count(anim->find_track(tracks_tf[max_keys]));
       i++) {
    float min_time =
        anim->track_get_key_time(anim->find_track(tracks_tf[0]), i);
    for (int p = 0; p < tracks_tf.size(); p++) {
      if (anim->track_get_key_time(anim->find_track(tracks_tf[p]), i) <
          min_time) {
        min_time = anim->track_get_key_time(anim->find_track(tracks_tf[p]), i);
      }
    }
    for (int p = 0; p < tracks_tf.size(); p++) {
      if (anim->track_get_key_time(anim->find_track(tracks_tf[p]), i) >
          min_time) {
        Vector3 t1;
        Quat t2;
        Vector3 t3;
        anim->transform_track_interpolate(anim->find_track(tracks_tf[p]),
                                          min_time, &t1, &t2, &t3);
        anim->transform_track_insert_key(anim->find_track(tracks_tf[p]),
                                         min_time, t1, t2, t3);
      }
    }
  }

  return anim->track_get_key_count(anim->find_track(tracks_tf[max_keys]));
}

#endif
