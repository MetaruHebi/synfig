/* === S Y N F I G ========================================================= */
/*!	\file dock_toolbox.cpp
**	\brief writeme
**
**	\legal
**	Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
**	Copyright (c) 2007, 2008 Chris Moore
**  Copyright (c) 2008 Paul Wise
**	Copyright (c) 2009 Nikita Kitaev
**
**	This file is part of Synfig.
**
**	Synfig is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 2 of the License, or
**	(at your option) any later version.
**
**	Synfig is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with Synfig.  If not, see <https://www.gnu.org/licenses/>.
**	\endlegal
**
** ========================================================================= */

/* === H E A D E R S ======================================================= */

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <gui/docks/dock_toolbox.h>

#include <gtkmm/accelmap.h>
#include <gtkmm/paned.h>
#include <gtkmm/stock.h>
#include <gtkmm/toolpalette.h>

#include <gui/statemanager.h>
#include <gui/app.h>
#include <gui/canvasview.h>
#include <gui/docks/dialog_tooloptions.h>
#include <gui/instance.h>
#include <gui/localization.h>
#include <gui/widgets/widget_defaults.h>

#include <synfig/general.h>

#include <synfigapp/main.h>

#endif

using namespace synfig;
using namespace studio;

/* === M A C R O S ========================================================= */

/* === G L O B A L S ======================================================= */

/* === P R O C E D U R E S ================================================= */

/* === M E T H O D S ======================================================= */


Dock_Toolbox::Dock_Toolbox():
	Dockable("toolbox",_("Toolbox"),"about_icon")
{
	set_use_scrolled(false);
	set_size_request(-1,-1);

	tool_item_group = manage(new class Gtk::ToolItemGroup());
	gtk_tool_item_group_set_label(tool_item_group->gobj(), nullptr);

	Gtk::ToolPalette *palette = manage(new Gtk::ToolPalette());
	palette->add(*tool_item_group);
	palette->set_expand(*tool_item_group);
	palette->set_exclusive(*tool_item_group, true);
	palette->set_icon_size(Gtk::IconSize::from_name("synfig-small_icon_16x16"));
	// let the palette propagate the scroll events
	palette->add_events(Gdk::SCROLL_MASK);
	palette->show();

	Gtk::ScrolledWindow *scrolled_window = manage(new Gtk::ScrolledWindow());
	scrolled_window->add(*palette);
	scrolled_window->set_border_width(2);
	scrolled_window->show();

	Widget_Defaults* widget_defaults(manage(new Widget_Defaults()));

	tool_box_paned = manage(new Gtk::Paned(Gtk::ORIENTATION_VERTICAL));
	tool_box_paned->pack1(*scrolled_window, Gtk::PACK_EXPAND_WIDGET|Gtk::PACK_SHRINK, false);
	tool_box_paned->pack2(*widget_defaults, Gtk::PACK_EXPAND_WIDGET|Gtk::PACK_SHRINK, false);
	tool_box_paned->set_position(200);
	tool_box_paned->show_all();
	add(*tool_box_paned);

	App::signal_canvas_view_focus().connect(
		sigc::hide(
			sigc::mem_fun(*this,&studio::Dock_Toolbox::update_tools) ));

	std::vector<Gtk::TargetEntry> listTargets;
	listTargets.push_back( Gtk::TargetEntry("text/plain") );
	listTargets.push_back( Gtk::TargetEntry("image") );
//	listTargets.push_back( Gtk::TargetEntry("image/x-sif") );

	drag_dest_set(listTargets);
	signal_drag_data_received().connect( sigc::mem_fun(*this, &studio::Dock_Toolbox::on_drop_drag_data_received) );

	changing_state_=false;

	App::signal_present_all().connect(sigc::mem_fun0(*this,&Dock_Toolbox::present));
}

Dock_Toolbox::~Dock_Toolbox()
{
	hide();
	//studio::App::cb.task(_("Toolbox: I was nailed!"));
	//studio::App::quit();

	if (studio::App::dock_toolbox == this)
		studio::App::dock_toolbox = nullptr;
}

void Dock_Toolbox::write_layout_string(std::string& params) const
{
	char num_str[6];
	snprintf(num_str, 5, "%d", tool_box_paned->get_position());
	params += std::string(num_str);
}

void Dock_Toolbox::read_layout_string(const std::string& params) const
{
	try {
		int pos = std::stoi(params.c_str());
		tool_box_paned->set_position(pos);
	} catch (...) {
		// ignores invalid value and let it use the default one
	}
}

void
Dock_Toolbox::set_active_state(const synfig::String& statename)
{
	changing_state_ = true;

	synfigapp::Main::set_state(statename);


	changing_state_ = false;
}

void
Dock_Toolbox::change_state(const synfig::String& statename, bool force)
{
	etl::handle<studio::CanvasView> canvas_view(studio::App::get_selected_canvas_view());
	if(canvas_view)
	{
		if(!force && statename==canvas_view->get_smach().get_state_name())
		{
			return;
		}

		if(state_button_map.count(statename))
		{
			state_button_map[statename]->activate();
		}
		else
		{
			synfig::error("Unknown state \"%s\"",statename.c_str());
		}
	}
}

void
Dock_Toolbox::change_state_(const Smach::state_base *state)
{
	if(changing_state_)
		return;
	changing_state_=true;

	try
	{
		etl::handle<studio::CanvasView> canvas_view(studio::App::get_selected_canvas_view());
		if(canvas_view)
				canvas_view->get_smach().enter(state);
		else
			refresh();
	}
	catch(...)
	{
		changing_state_=false;
		throw;
	}

	changing_state_=false;
}


/*! \fn Dock_Toolbox::add_state(const Smach::state_base *state)
 *  \brief Add and connect a toggle button to the toolbox defined by a state
 *  \param state a const pointer to Smach::state_base
*/
void
Dock_Toolbox::add_state(const Smach::state_base *state)
{
	assert(state);

	String name=state->get_name();

	Gtk::RadioToolButton *tool_button = manage(new Gtk::RadioToolButton());
	tool_button->set_group(radio_tool_button_group);
	Glib::RefPtr<Gtk::Action> related_action = App::get_state_manager()->get_action_group()->get_action("state-"+name);
	tool_button->set_related_action(related_action);
	related_action->property_tooltip() = "";

	// Keeps updating the tooltip if user changes the shortcut at runtime
	tool_button->property_has_tooltip() = true;
	tool_button->signal_query_tooltip().connect([name](int,int,bool,const Glib::RefPtr<Gtk::Tooltip>& tooltip) -> bool
	{
		Gtk::StockItem stock_item;
		if (Gtk::Stock::lookup(Gtk::StockID("synfig-"+name), stock_item)) {
			std::string tooltip_string = stock_item.get_label();

			Gtk::AccelKey key;
			if (Gtk::AccelMap::lookup_entry("<Actions>/action_group_state_manager/state-" + name, key)) {
				tooltip_string += "  ";
				tooltip_string += gtk_accelerator_get_label(key.get_key(), GdkModifierType(key.get_mod()));
			}

			tooltip->set_text(tooltip_string);
		} else {
			synfig::warning("There is no StockItem named 'synfig-%s", name.c_str());
		}

		return true;
	});

	tool_button->show();

	tool_item_group->insert(*tool_button);
	tool_item_group->show_all();

	state_button_map[name] = tool_button;


	refresh();
}


void
Dock_Toolbox::update_tools()
{
	etl::handle<Instance> instance = App::get_selected_instance();
	CanvasView::Handle canvas_view = App::get_selected_canvas_view();

	// Disable buttons if there isn't any open document instance
	bool sensitive = instance && canvas_view;
	for (const auto& item : state_button_map)
		item.second->set_sensitive(sensitive);

	if (canvas_view && canvas_view->get_smach().get_state_name())
		set_active_state(canvas_view->get_smach().get_state_name());
	else
		set_active_state("none");
}


void
Dock_Toolbox::refresh()
{
	update_tools();
}


void
Dock_Toolbox::on_drop_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& context, int /*x*/, int /*y*/, const Gtk::SelectionData& selection_data_, guint /*info*/, guint time)
{
	// We will make this true once we have a solid drop
	bool success(false);

	if ((selection_data_.get_length() >= 0) && (selection_data_.get_format() == 8))
	{
		synfig::String selection_data((gchar *)(selection_data_.get_data()));

		// For some reason, GTK hands us a list of URLs separated
		// by not only Carriage-Returns, but also Line-Feeds.
		// Line-Feeds will mess us up. Remove all the line-feeds.
		while(selection_data.find_first_of('\r')!=synfig::String::npos)
			selection_data.erase(selection_data.begin()+selection_data.find_first_of('\r'));

		std::stringstream stream(selection_data);

		while(stream)
		{
			synfig::String filename,URI;
			getline(stream,filename);

			// If we don't have a filename, move on.
			if(filename.empty())
				continue;

			// Make sure this URL is of the "file://" type.
			URI=String(filename.begin(),filename.begin()+sizeof("file://")-1);
			if(URI!="file://")
			{
				synfig::warning("Unknown URI (%s) in \"%s\"",URI.c_str(),filename.c_str());
				continue;
			}

			// Strip the "file://" part from the filename
			filename=synfig::String(filename.begin()+sizeof("file://")-1,filename.end());

			synfig::info("Attempting to open "+filename);
			if(App::open(filename))
				success=true;
			else
				synfig::error("Drop failed: Unable to open "+filename);
		}
	}
	else
		synfig::error("Drop failed: bad selection data");

	// Finish the drag
	context->drag_finish(success, false, time);
}
