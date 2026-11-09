// Playlist Operations - playlist operations for foobar2000.

#include <SDK/foobar2000.h>
#include <SDK/messageBox.h>
#include <memory>
#include <utility>
#include <vector>

// Stamped by ./version.sh from resource/version.txt - do not edit by hand.
DECLARE_COMPONENT_VERSION("Playlist Operations", "0.1.0",
    "Playlist operations for foobar2000.\n"
    "Adds Add to..., Move to..., and Remove from all playlists... to the track context menu, plus a command that jumps to the top of the active playlist.\n"
    "\n"
    "Report bugs at github.com/aro-1790/foo_plops\n"
    "\n"
    "Copyright (c) 2026, Adam O'Dell.\n"
    "Distributed under the BSD 3-Clause License.");

VALIDATE_COMPONENT_FILENAME("foo_plops.dll");

static_assert(FOOBAR2000_TARGET_VERSION >= 81, "foo_plops targets the foobar2000 2.0 API");

namespace {

constexpr GUID guid_playlist_group = {
    0x4dbd3a7a, 0xbedb, 0x4926, { 0x86, 0x72, 0x2c, 0xca, 0x7d, 0xba, 0x0c, 0x1e }
};
constexpr GUID guid_move_command = {
    0x4c6bd2e0, 0x49bb, 0x4aaf, { 0x92, 0x13, 0x00, 0x3c, 0xfa, 0xd5, 0x5a, 0xc4 }
};
constexpr GUID guid_add_command = {
    0xa03d0095, 0x89ed, 0x4d9d, { 0x9d, 0xe9, 0x76, 0x36, 0xc2, 0x04, 0x00, 0x3c }
};
constexpr GUID guid_remove_all_command = {
    0xd2f4a1c8, 0x7e56, 0x4b9d, { 0xa3, 0xf0, 0x5c, 0x8e, 0x1d, 0x27, 0x4b, 0x60 }
};
constexpr GUID guid_mainmenu_group = {
    0x7a4c9e21, 0x35d8, 0x4f62, { 0x90, 0x6b, 0x14, 0xe3, 0x5c, 0x82, 0x71, 0xd4 }
};
constexpr GUID guid_go_to_top_command = {
    0x2f8b6d13, 0x904a, 0x47c5, { 0xbe, 0x51, 0x72, 0x0a, 0x39, 0xd6, 0x18, 0x5f }
};

enum class operation_t { move, add };

static void report_error(const char * message) {
    popup_message::g_show(message, "Playlist Operations", popup_message::icon_error);
}

static void report_failure() {
    report_error("The playlist operation could not be completed. The playlist may be locked.");
}

static void execute_operation(operation_t operation, const GUID & target_guid,
    metadb_handle_list_cref data) {
    if (data.get_count() == 0) return;

    auto api = playlist_manager_v5::get();
    const t_size source = api->get_active_playlist();
    const t_size target = api->find_playlist_by_guid(target_guid);
    if (source == SIZE_MAX) {
        report_error("No playlist is active.");
        return;
    }
    if (target == SIZE_MAX) {
        report_error("The target playlist no longer exists.");
        return;
    }
    // A shortcut mapped while another playlist was active can point at this one.
    if (operation == operation_t::move && source == target) return;

    pfc::bit_array_bittable selection(api->playlist_get_item_count(source));
    api->playlist_get_selection_mask(source, selection);

    metadb_handle_list items;
    api->playlist_get_items(source, items, selection);
    if (items.get_count() != data.get_count()) {
        report_error("The playlist selection changed before the command ran.");
        return;
    }

    if (operation == operation_t::add) {
        if (api->playlist_lock_get_filter_mask(target) & playlist_lock::filter_add) {
            report_failure();
            return;
        }
        api->playlist_undo_backup(target);
        if (api->playlist_insert_items(target, SIZE_MAX, items, bit_array_false()) == SIZE_MAX)
            report_failure();
        return;
    }

    if ((api->playlist_lock_get_filter_mask(source) & playlist_lock::filter_remove) ||
        (api->playlist_lock_get_filter_mask(target) & playlist_lock::filter_add)) {
        report_failure();
        return;
    }

    api->playlist_undo_backup(target);
    const t_size inserted_at = api->playlist_insert_items(
        target, SIZE_MAX, items, bit_array_false());
    if (inserted_at == SIZE_MAX) {
        report_failure();
        return;
    }
    api->playlist_undo_backup(source);
    if (!api->playlist_remove_items(source, selection)) {
        // Leave the target untouched when the source removal fails.
        api->playlist_remove_items(target, bit_array_range(inserted_at, items.get_count()));
        report_failure();
    }
}

static void go_to_top_of_active_playlist() {
    auto api = playlist_manager::get();
    const t_size playlist = api->get_active_playlist();
    if (playlist == SIZE_MAX || api->playlist_get_item_count(playlist) == 0) return;

    api->playlist_set_focus_item(playlist, 0);
    api->playlist_set_selection(playlist, pfc::bit_array_true(), pfc::bit_array_one(0));
    api->playlist_ensure_visible(playlist, 0);
}

static void remove_from_all_playlists(metadb_handle_list_cref data) {
    if (data.get_count() == 0) return;
    if (fb2k::messageBox(core_api::get_main_window(),
        "Remove the selected items from every playlist?\n\n"
        "This removes playlist entries only; files will not be deleted from disk.",
        "Playlist Operations", MB_YESNO | MB_ICONWARNING) != IDYES)
        return;

    auto api = playlist_manager_v5::get();
    metadb_handle_list targets;
    targets.add_items(data);
    targets.sort_by_pointer();

    t_size changed = 0;
    t_size skipped = 0;
    t_size failed = 0;
    const t_size playlist_count = api->get_playlist_count();
    for (t_size playlist = 0; playlist < playlist_count; ++playlist) {
        const t_size item_count = api->playlist_get_item_count(playlist);
        pfc::bit_array_bittable mask(item_count);
        metadb_handle_list items;
        api->playlist_get_items(playlist, items, pfc::bit_array_true());

        bool found = false;
        for (t_size item = 0; item < items.get_count(); ++item) {
            if (targets.bsearch_by_pointer(items[item]) != SIZE_MAX) {
                mask.set(item, true);
                found = true;
            }
        }
        if (!found) continue;
        if (api->playlist_lock_get_filter_mask(playlist) & playlist_lock::filter_remove) {
            ++skipped;
            continue;
        }

        api->playlist_undo_backup(playlist);
        if (api->playlist_remove_items(playlist, mask))
            ++changed;
        else
            ++failed;
    }

    pfc::string8 summary;
    if (changed != 0)
        summary << "Removed selected items from " << changed << " playlist(s).";
    else
        summary << "No playlist entries were removed.";
    if (skipped != 0) summary << " Skipped " << skipped << " locked playlist(s).";
    if (failed != 0) summary << " Failed for " << failed << " playlist(s).";
    popup_message::g_showToast(summary);
}

class playlist_leaf_node : public contextmenu_item_node_leaf {
public:
    playlist_leaf_node(operation_t operation, pfc::string8 name, GUID playlist_guid)
        : m_operation(operation), m_name(std::move(name)), m_playlist_guid(playlist_guid) {}

    bool get_display_data(pfc::string_base & out, unsigned & flags,
        metadb_handle_list_cref, const GUID &) override {
        flags = 0;
        out = m_name;
        return true;
    }

    void execute(metadb_handle_list_cref data, const GUID &) override {
        execute_operation(m_operation, m_playlist_guid, data);
    }

    bool get_description(pfc::string_base & out) override {
        pfc::string_formatter text;
        text << (m_operation == operation_t::move ? "Move selected items to " :
            "Add selected items to ") << m_name;
        out = text;
        return true;
    }

    GUID get_guid() override { return m_playlist_guid; }
    bool is_mappable_shortcut() override { return true; }

private:
    const operation_t m_operation;
    const pfc::string8 m_name;
    const GUID m_playlist_guid;
};

class playlist_popup_node : public contextmenu_item_node_root_popup {
public:
    explicit playlist_popup_node(operation_t operation) : m_operation(operation) {
        auto api = playlist_manager_v5::get();
        const t_size count = api->get_playlist_count();
        // Moving onto the active playlist is a no-op, so it is not offered.
        const t_size source = m_operation == operation_t::move ? api->get_active_playlist() : SIZE_MAX;
        // Every move would be refused, so the popup keeps itself out of the menu.
        if (m_operation == operation_t::move && (source == SIZE_MAX ||
            (api->playlist_lock_get_filter_mask(source) & playlist_lock::filter_remove)))
            return;
        m_children.reserve(count);
        for (t_size index = 0; index < count; ++index) {
            if (index == source) continue;
            // Locked destinations refuse the insert; autoplaylists are locked.
            if (api->playlist_lock_get_filter_mask(index) & playlist_lock::filter_add) continue;
            pfc::string8 name;
            api->playlist_get_name(index, name);
            m_children.push_back(std::make_unique<playlist_leaf_node>(
                operation, name, api->playlist_get_guid(index)));
        }
    }

    bool get_display_data(pfc::string_base & out, unsigned & flags,
        metadb_handle_list_cref, const GUID & caller) override {
        if (m_children.empty()) return false;
        if (caller == contextmenu_item::caller_now_playing ||
            caller == contextmenu_item::caller_media_library_viewer ||
            caller == contextmenu_item::caller_active_playlist)
            return false;

        flags = 0;
        out = m_operation == operation_t::move ? "Move to..." : "Add to...";
        return true;
    }

    t_size get_children_count() override { return m_children.size(); }
    contextmenu_item_node * get_child(t_size index) override { return m_children[index].get(); }
    GUID get_guid() override { return pfc::guid_null; }
    bool is_mappable_shortcut() override { return false; }

private:
    const operation_t m_operation;
    std::vector<std::unique_ptr<playlist_leaf_node>> m_children;
};

class playlist_operations_service : public contextmenu_item_v2 {
public:
    unsigned get_num_items() override { return 2; }
    t_enabled_state get_enabled_state(unsigned) override { return DEFAULT_ON; }

    void get_item_name(unsigned index, pfc::string_base & out) override {
        out = index == 0 ? "Add to..." : "Move to...";
    }

    GUID get_item_guid(unsigned index) override {
        return index == 0 ? guid_add_command : guid_move_command;
    }

    bool get_item_description(unsigned index, pfc::string_base & out) override {
        out = index == 0 ? "Copy selected items to another playlist." :
            "Move selected items to another playlist.";
        return true;
    }

    GUID get_parent() override { return guid_playlist_group; }

    contextmenu_item_node_root * instantiate_item(unsigned index,
        metadb_handle_list_cref, const GUID &) override {
        return new playlist_popup_node(index == 0 ? operation_t::add : operation_t::move);
    }

    void item_execute_simple(unsigned index, const GUID & node,
        metadb_handle_list_cref data, const GUID &) override {
        if (node == pfc::guid_null) return;
        execute_operation(index == 0 ? operation_t::add : operation_t::move, node, data);
    }
};

class remove_from_all_service : public contextmenu_item_simple {
public:
    unsigned get_num_items() override { return 1; }

    void get_item_name(unsigned, pfc::string_base & out) override {
        out = "Remove from all playlists...";
    }

    void context_command(unsigned, metadb_handle_list_cref data, const GUID &) override {
        remove_from_all_playlists(data);
    }

    GUID get_item_guid(unsigned) override { return guid_remove_all_command; }

    bool get_item_description(unsigned, pfc::string_base & out) override {
        out = "Remove the selected items from every playlist without deleting files.";
        return true;
    }

    bool context_get_display(unsigned, metadb_handle_list_cref, pfc::string_base & out,
        unsigned & displayflags, const GUID & caller) override {
        displayflags = 0;
        if (caller == contextmenu_item::caller_active_playlist ||
            caller == contextmenu_item::caller_playlist_manager)
            return false;
        out = "Remove from all playlists...";
        return true;
    }

    GUID get_parent() override { return guid_playlist_group; }
};

class playlist_operations_menu_service : public mainmenu_commands {
public:
    t_uint32 get_command_count() override { return 1; }

    GUID get_command(t_uint32) override { return guid_go_to_top_command; }

    void get_name(t_uint32, pfc::string_base & out) override {
        out = "Go to top of active playlist";
    }

    bool get_description(t_uint32, pfc::string_base & out) override {
        out = "Select the first item in the active playlist and scroll it into view.";
        return true;
    }

    bool get_display(t_uint32 index, pfc::string_base & out, t_uint32 & flags) override {
        get_name(index, out);
        flags = 0;
        auto api = playlist_manager::get();
        const t_size playlist = api->get_active_playlist();
        if (playlist == SIZE_MAX || api->playlist_get_item_count(playlist) == 0)
            flags |= flag_disabled;
        return true;
    }

    GUID get_parent() override { return guid_mainmenu_group; }

    void execute(t_uint32, service_ptr_t<service_base>) override {
        go_to_top_of_active_playlist();
    }
};

static contextmenu_group_popup_factory g_playlist_group(
    guid_playlist_group, contextmenu_groups::root, "Playlist Operations", 0);
static contextmenu_item_factory_t<playlist_operations_service> g_playlist_operations;
static contextmenu_item_factory_t<remove_from_all_service> g_remove_from_all;

static mainmenu_group_popup_factory g_mainmenu_group(
    guid_mainmenu_group, mainmenu_groups::edit_part1,
    mainmenu_commands::sort_priority_base, "Playlist Operations");
static mainmenu_commands_factory_t<playlist_operations_menu_service> g_mainmenu_commands;

} // namespace
