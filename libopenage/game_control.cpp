// Copyright 2015-2016 the openage authors. See copying.md for legal info.

#include "gamestate/game_spec.h"
#include "util/strings.h"
#include "game_control.h"

namespace openage {

CreateMode::CreateMode()
	:
	selected{0},
	setting_value{false} {

	auto &action = Engine::get().get_action_manager();
	// action bindings
	this->bind(action.get("START_GAME"), [this](const input::action_arg_t &) {
		Engine &engine = Engine::get();
		options::OptionNode *node = engine.get_child("Generator");
		if (!node) {
			return;
		}

		std::vector<std::string> list = node->list_variables();
		std::vector<std::string> flist = node->list_functions();
		unsigned int size = list.size() + flist.size();
		unsigned int selected = util::mod<int>(this->selected, size);

		if (this->setting_value) {

			// modifying a value
			std::vector<std::string> list = node->list_variables();
			std::string selected_name = list[selected];
			auto &var = node->get_variable_rw(selected_name);

			// try change the value
			// deal with number parsing exceptions
			try {
				if (var.type == options::option_type::list_type) {
					auto list = var.value<options::option_list>();
					list.push_back(this->new_value);
					var = list;
					log::log(MSG(dbg) << selected_name << " appended with " << this->new_value);
				}
				else {
					var = options::parse(var.type, this->new_value);
					log::log(MSG(dbg) << selected_name << " set to " << this->new_value);
				}
			}
			catch (const std::invalid_argument &e) {
				log::log(MSG(dbg) << "exception setting " << selected_name << " to " << this->new_value);
			}
			catch (const std::out_of_range &e) {
				log::log(MSG(dbg) << "exception setting " << selected_name << " to " << this->new_value);
			}

			this->setting_value = false;
			this->utf8_mode = false;
			this->new_value = "";
		}
		else {
			if (selected >= list.size()) {
				this->response_value = node->do_action(flist[selected - list.size()]).str_value();
				log::log(MSG(dbg) << this->response_value);
			}
			else {
				this->setting_value = true;
				this->utf8_mode = true;
				this->new_value = "";
			}
		}

	});
	this->bind(action.get("UP_ARROW"), [this](const input::action_arg_t &) {
		if (!this->setting_value) {
			this->selected -= 1;
		}
	});
	this->bind(action.get("DOWN_ARROW"), [this](const input::action_arg_t &) {
		if (!this->setting_value) {
			this->selected += 1;
		}
	});
	this->bind(input::event_class::UTF8, [this](const input::action_arg_t &arg) {
		if (this->setting_value) {
			this->new_value += arg.e.as_utf8();
			return true;
		}
		return false;
	});
}

bool CreateMode::available() const {
	return true;
}

std::string CreateMode::name() const {
	return "Creation Mode";
}


void CreateMode::on_enter() {}

void CreateMode::render() {
	Engine &engine = Engine::get();

	// initial draw positions
	int x = 75;
	int y = engine.engine_coord_data->window_size.y - 65;

	options::OptionNode *node = engine.get_child("Generator");
	if (node) {
		unsigned int i = 0;


		// list everything
		std::vector<std::string> list = node->list_variables();
		std::vector<std::string> flist = node->list_functions();
		unsigned int size = list.size() + flist.size();
		unsigned int selected = util::mod<int>(this->selected, size);

		for (auto &s : list) {
			engine.render_text({x, y}, 20, "%s", s.c_str());
			if (i == selected) {
				engine.render_text({x - 35, y}, 20, ">>");
			}
			auto var = node->get_variable(s);
			engine.render_text({x + 320, y}, 20, "%s", var.str_value().c_str());
			y -= 24;
			i++;
		}
		for (auto &s : flist) {
			engine.render_text({x, y}, 20, "%s", (s + "()").c_str());
			if (i == selected) {
				engine.render_text({x - 35, y}, 20, ">>");
			}
			y -= 24;
			i++;
		}

		if (this->setting_value) {
			y -= 36;
			engine.render_text({x + 45, y}, 20, "set value:");
			engine.render_text({x + 320, y}, 20, "%s", this->new_value.c_str());
		}
		engine.render_text({0, 100}, 20, "%s", this->response_value.c_str());
	}
	else {
		engine.render_text({0, 100}, 20, "Error: No generator");
	}
}

ActionMode::ActionMode()
	:
	use_set_ability{false},
	type_focus{nullptr},
	rng{0} {

	auto &action = Engine::get().get_action_manager();
	this->bind(action.get("TRAIN_OBJECT"), [this](const input::action_arg_t &) {

		// attempt to train editor selected object
		Engine &engine = Engine::get();

		// randomly select between male and female villagers
		auto player = engine.player_focus();
		auto type = player->get_type(this->rng.probability(0.5)? 83 : 293);
		Command cmd(*player, type);
		cmd.add_flag(command_flag::direct);
		this->selection.all_invoke(cmd);
	});
	this->bind(action.get("ENABLE_BUILDING_PLACEMENT"), [this](const input::action_arg_t &) {
		// this->building_placement = true;
	});
	this->bind(action.get("DISABLE_SET_ABILITY"), [this](const input::action_arg_t &) {
		this->use_set_ability = false;
	});
	this->bind(action.get("SET_ABILITY_MOVE"), [this](const input::action_arg_t &) {
		this->use_set_ability = true;
		this->ability = ability_type::move;
	});
	this->bind(action.get("SET_ABILITY_GATHER"), [this](const input::action_arg_t &) {
		this->use_set_ability = true;
		this->ability = ability_type::gather;
	});
	this->bind(action.get("SET_ABILITY_GARRISON"), [this](const input::action_arg_t &) {
		this->use_set_ability = true;
		this->ability = ability_type::garrison;
	});
	this->bind(action.get("SPAWN_VILLAGER"), [this](const input::action_arg_t &) {
		Engine &engine = Engine::get();
		auto player = engine.player_focus();
		if (player->type_count() > 0) {
			UnitType &type = *player->get_type(590);

			// TODO tile position
			engine.get_game()->placed_units.new_unit(type, *player, this->mousepos_phys3);
		}
	});
	this->bind(action.get("KILL_UNIT"), [this](const input::action_arg_t &) {
		selection.kill_unit();
	});

	// Villager build commands
	// TODO place this into separate building menus instead of global hotkeys
	auto bind_building_key = [this](input::action_t action, int building) {
		this->bind(action, [this, building](const input::action_arg_t &) {
			if (this->selection.contains_builders()) {
				Engine &engine = Engine::get();
				auto player = engine.player_focus();
				this->type_focus = player->get_type(building);
				if (&engine.get_input_manager().get_top_context() != &this->building_context) {
					engine.get_input_manager().register_context(&this->building_context);
				}
			}
		});
	};
	bind_building_key(action.get("BUILDING_1"), 70); // House
	bind_building_key(action.get("BUILDING_2"), 68); // Mill
	bind_building_key(action.get("BUILDING_3"), 584); // Mining camp
	bind_building_key(action.get("BUILDING_4"), 562); // Lumber camp
	bind_building_key(action.get("BUILDING_5"), 12); // barracks
	bind_building_key(action.get("BUILDING_6"), 87); // archery range
	bind_building_key(action.get("BUILDING_7"), 101); // stable
	bind_building_key(action.get("BUILDING_8"), 49); // siege workshop
	bind_building_key(action.get("BUILDING_TOWN_CENTER"), 109); // Town center

	auto bind_build = [this](input::action_t action, const bool increase) {
		this->building_context.bind(action, [this, increase](const input::action_arg_t &arg) {
			auto mousepos_camgame = arg.mouse.to_camgame();
			this->mousepos_phys3 = mousepos_camgame.to_phys3();
			this->mousepos_tile = this->mousepos_phys3.to_tile3().to_tile();

			this->place_selection(this->mousepos_phys3);
			if (!increase) {
				this->type_focus = nullptr;
				Engine::get().get_input_manager().remove_context(&this->building_context);
			}
		});
	};

	bind_build(action.get("BUILD"), false);
	bind_build(action.get("KEEP_BUILDING"), true);

	auto bind_select = [this](input::action_t action, const bool increase) {
		this->bind(action, [this, increase](const input::action_arg_t &arg) {
			auto mousepos_camgame = arg.mouse.to_camgame();
			Engine &engine = Engine::get();
			Terrain *terrain = engine.get_game()->terrain.get();
			this->selection.drag_update(mousepos_camgame);
			this->selection.drag_release(terrain, increase);
		});
	};

	bind_select(action.get("SELECT"), false);
	bind_select(action.get("INCREASE_SELECTION"), true);

	this->bind(action.get("ORDER_SELECT"), [this](const input::action_arg_t &arg) {
		if (this->type_focus) {
			// right click can cancel building placement
			this->type_focus = nullptr;
			Engine::get().get_input_manager().remove_context(&this->building_context);
		}
		auto mousepos_camgame = arg.mouse.to_camgame();
		auto mousepos_phys3 = mousepos_camgame.to_phys3();

		auto cmd = this->get_action(mousepos_phys3);
		cmd.add_flag(command_flag::direct);
		this->selection.all_invoke(cmd);
		this->use_set_ability = false;
	});

	this->bind(input::event_class::MOUSE, [this](const input::action_arg_t &arg) {
		Engine &engine = Engine::get();

		auto mousepos_camgame = arg.mouse.to_camgame();
		this->mousepos_phys3 = mousepos_camgame.to_phys3();
		this->mousepos_tile = this->mousepos_phys3.to_tile3().to_tile();

		// drag selection box
		if (arg.e.cc == input::ClassCode(input::event_class::MOUSE_MOTION, 0) &&
			engine.get_input_manager().is_down(input::event_class::MOUSE_BUTTON, 1) && !this->type_focus) {
			this->selection.drag_update(mousepos_camgame);
			return true;
		}
		return false;
	});
}

bool ActionMode::available() const {
	Engine &engine = Engine::get();
	if (engine.get_game()) {
		return true;
	}
	else {
		log::log(MSG(warn) << "Cannot enter action mode without a game");
		return false;
	}
}

void ActionMode::on_enter() {
	this->selection.clear();
}

Command ActionMode::get_action(const coord::phys3 &pos) const {
	Engine &engine = Engine::get();
	GameMain *game = engine.get_game();
	auto obj = game->terrain->obj_at_point(pos);
	if (obj) {
		Command c(*engine.player_focus(), &obj->unit, pos);
		if (this->use_set_ability) {
			c.set_ability(ability);
		}
		return c;
	}
	else {
		Command c(*engine.player_focus(), pos);
		if (this->use_set_ability) {
			c.set_ability(ability);
		}
		return c;
	}
}

bool ActionMode::place_selection(coord::phys3 point) {
	if (this->type_focus) {

		// confirm building placement with left click
		// first create foundation using the producer
		Engine &engine = Engine::get();
		UnitContainer *container = &engine.get_game()->placed_units;
		UnitReference new_building = container->new_unit(*this->type_focus, *engine.player_focus(), point);

		// task all selected villagers to build
		// TODO: editor placed objects are completed already
		if (new_building.is_valid()) {
			Command cmd(*engine.player_focus(), new_building.get());
			cmd.set_ability(ability_type::build);
			cmd.add_flag(command_flag::direct);
			this->selection.all_invoke(cmd);
			return true;
		}
	}
	return false;
}

void ActionMode::render() {
	Engine &engine = Engine::get();
	if (engine.get_game()) {
		Player *player = engine.player_focus();
		// draw actions mode indicator
		int x = 5;
		int y = engine.engine_coord_data->window_size.y - 25;

		std::string status;
		status += "Food: " + std::to_string(static_cast<int>(player->amount(game_resource::food)));
		status += " | Wood: " + std::to_string(static_cast<int>(player->amount(game_resource::wood)));
		status += " | Gold: " + std::to_string(static_cast<int>(player->amount(game_resource::gold)));
		status += " | Stone: " + std::to_string(static_cast<int>(player->amount(game_resource::stone)));
		status += " | Actions mode";
		status += " ([" + std::to_string(player->color) + "] " + player->name + ")";
		if (this->use_set_ability) {
			status += " (" + std::to_string(this->ability) + ")";
		}
		engine.render_text({x, y}, 20, "%s", status.c_str());


		// when a building is being placed
		if (this->type_focus) {
			auto txt = this->type_focus->default_texture();
			auto size = this->type_focus->foundation_size;
			tile_range center = building_center(this->mousepos_phys3, size);
			txt->sample(center.draw.to_camgame().to_window().to_camhud(), player->color);
		}
	}
	else {
		engine.render_text({0, 140}, 12, "Action Mode requires a game");
	}

	this->selection.on_drawhud();
}

std::string ActionMode::name() const {
	return "Action Mode";
}


EditorMode::EditorMode()
	:
	editor_current_terrain{0},
	editor_current_type{0},
	editor_category{0},
	selected_type{nullptr},
	selected_owner{nullptr},
	paint_terrain{true} {

	auto &action = Engine::get().get_action_manager();
	// bind required hotkeys
	this->bind(action.get("ENABLE_BUILDING_PLACEMENT"), [this](const input::action_arg_t &) {
		log::log(MSG(dbg) << "change category");
		Engine &engine = Engine::get();
		Player *player = engine.player_focus();
		std::vector<std::string> cat = player->civ->get_type_categories();
		if (this->paint_terrain) {

			// switch from terrain to first unit category
			this->paint_terrain = false;
			this->editor_category = 0;
		}
		else {
			this->editor_category += 1;

			// passed last category, switch back to terrain
			if (cat.size() <= this->editor_category) {
				this->paint_terrain = true;
				this->editor_category = 0;
			}
		}

		// update category string
		if (!cat.empty()) {
			this->category = cat[this->editor_category];
			std::vector<index_t> inds = player->civ->get_category(this->category);
			if (!inds.empty()) {
				this->editor_current_type = util::mod<ssize_t>(editor_current_type, inds.size());
				this->selected_type = player->get_type(inds[this->editor_current_type]);
				this->selected_owner = player;
			}
		}
	});

	auto bind_change_terrain = [this](const input::action_t action, const int direction) {
		this->bind(action, [this, direction](const input::action_arg_t) {
			Engine &engine = Engine::get();
			Player *player = engine.player_focus();
			GameSpec *spec = engine.get_game()->get_spec();

			// modify selected item
			if (this->paint_terrain) {
				editor_current_terrain = util::mod<ssize_t>(editor_current_terrain + direction, spec->get_terrain_meta()->terrain_id_count);
			} else if (player->type_count() > 0) {
				std::vector<index_t> inds = player->civ->get_category(this->category);
				if (!inds.empty()) {
					this->editor_current_type = util::mod<ssize_t>(editor_current_type + direction, inds.size());
					this->selected_type = player->get_type(inds[this->editor_current_type]);
					this->selected_owner = player;
				}
			}
		});
	};

	bind_change_terrain(action.get("FORWARD"), 1);
	bind_change_terrain(action.get("BACK"), -1);

	this->bind(input::event_class::MOUSE, [this](const input::action_arg_t &arg) {
		Engine &engine = Engine::get();
		if (arg.e.cc == input::ClassCode(input::event_class::MOUSE_BUTTON, 1) ||
			engine.get_input_manager().is_down(input::event_class::MOUSE_BUTTON, 1)) {
			if (this->paint_terrain) {
				this->paint_terrain_at(arg.mouse);
			} else {
				this->paint_entity_at(arg.mouse, false);
			}
			return true;
		}
		else if (arg.e.cc == input::ClassCode(input::event_class::MOUSE_BUTTON, 3) ||
			engine.get_input_manager().is_down(input::event_class::MOUSE_BUTTON, 3)) {
				if (!this->paint_terrain) {
					this->paint_entity_at(arg.mouse, true);
				}
			return true;
		}
		return false;
	});
}

bool EditorMode::available() const {
	Engine &engine = Engine::get();
	if (engine.get_game()) {
		return true;
	}
	else {
		log::log(MSG(warn) << "Cannot enter editor mode without a game");
		return false;
	}
}

void EditorMode::on_enter() {}

void EditorMode::render() {

	// Get the current game
	Engine &engine = Engine::get();
	GameMain *game = engine.get_game();

	if (game) {
		coord::window text_pos{12, engine.engine_coord_data->window_size.y - 24};

		// draw the currently selected editor item
		if (this->paint_terrain && game->terrain) {
			coord::window bpreview_pos{63, 84};
			game->terrain->texture(this->editor_current_terrain)->draw(bpreview_pos.to_camhud(), ALPHAMASKED);
			engine.render_text(text_pos, 20, "%s", ("Terrain " + std::to_string(this->editor_current_terrain)).c_str());
		}
		else if (this->selected_type) {
			// and the current active building
			auto txt = this->selected_type->default_texture();
			coord::window bpreview_pos {163, 154};
			txt->sample(bpreview_pos.to_camhud(), this->selected_owner->color);

			std::string selected_str = this->selected_owner->name + ": " + this->category + " - " + this->selected_type->name();
			engine.render_text(text_pos, 20, "%s", selected_str.c_str());
		}
	}
	else {
		engine.render_text({0, 140}, 12, "Editor Mode requires a game");
	}
}

std::string EditorMode::name() const {
	return "Editor mode";
}

void EditorMode::paint_terrain_at(const coord::window &point) {
	Engine &engine = Engine::get();
	Terrain *terrain = engine.get_game()->terrain.get();

	auto mousepos_camgame = point.to_camgame();
	auto mousepos_phys3 = mousepos_camgame.to_phys3();
	auto mousepos_tile = mousepos_phys3.to_tile3().to_tile();

	TerrainChunk *chunk = terrain->get_create_chunk(mousepos_tile);
	chunk->get_data(mousepos_tile)->terrain_id = editor_current_terrain;
}

void EditorMode::paint_entity_at(const coord::window &point, const bool del) {
	Engine &engine = Engine::get();
	Terrain *terrain = engine.get_game()->terrain.get();

	auto mousepos_camgame = point.to_camgame();
	auto mousepos_phys3 = mousepos_camgame.to_phys3();
	auto mousepos_tile = mousepos_phys3.to_tile3().to_tile();

	TerrainChunk *chunk = terrain->get_create_chunk(mousepos_tile);
	// TODO : better detection of presence of unit
	if (!chunk->get_data(mousepos_tile)->obj.empty()) {
		if (del) {
			// delete first object currently standing at the clicked position
			TerrainObject *obj = chunk->get_data(mousepos_tile)->obj[0];
			obj->remove();
		}
	} else if (!del && this->selected_type) {
		// tile is empty so try creating a unit
		UnitContainer *container = &engine.get_game()->placed_units;
		container->new_unit(*this->selected_type, *this->selected_owner, mousepos_phys3);
	}
}


GameControl::GameControl(openage::Engine *engine)
	:
	engine{engine},
	active_mode_index{0} {

	// add handlers
	engine->register_drawhud_action(this);

	// modes list
	this->modes.push_back(std::make_unique<CreateMode>());
	this->modes.push_back(std::make_unique<EditorMode>());
	this->modes.push_back(std::make_unique<ActionMode>());

	// initial active mode
	this->active_mode = modes.front().get();
	engine->get_input_manager().register_context(this->active_mode);

	auto &action = engine->get_action_manager();
	auto &global_input_context = engine->get_input_manager().get_global_context();
	global_input_context.bind(action.get("TOGGLE_CONSTRUCT_MODE"), [this](const input::action_arg_t &) {
		this->toggle_mode();
	});

}

void GameControl::toggle_mode() {
	int next_mode = (this->active_mode_index + 1) % this->modes.size();
	if (this->modes[next_mode]->available()) {
		engine->get_input_manager().remove_context(this->active_mode);

		// set the new active mode
		this->active_mode_index = next_mode;
		this->active_mode = this->modes[next_mode].get();
		this->active_mode->on_enter();

		// update the context
		engine->get_input_manager().register_context(this->active_mode);
	}
}

bool GameControl::on_drawhud() {
	Engine &engine = Engine::get();
	int x = engine.engine_coord_data->window_size.x - 300;
	int y = engine.engine_coord_data->window_size.y - 24;
	engine.render_text({x, y}, 20, "%s", active_mode->name().c_str());

	auto binds = this->active_mode->active_binds();

	for (auto it = binds.begin() ; it != binds.end(); ++it) {
		y -= 14;
		engine.render_text({x, y}, 12, "%s", it->c_str());
	}

	y -= 20;
	engine.render_text({x, y}, 20, "Global Bindings");

	auto global_binds = engine.get_input_manager().get_global_context().active_binds();
	for (auto it = global_binds.begin() ; it != global_binds.end(); ++it) {
		y -= 14;
		engine.render_text({x, y}, 12, "%s", it->c_str());
	}

	// render the active mode
	this->active_mode->render();
	return true;
}


} // openage
