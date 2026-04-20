/**
 * @file test_entry_info.cpp
 * @brief Unit tests for entry_info tree helpers: tree_label, to_tree_node,
 *        flatten_paths, find_entry, expand_all.
 */
#include "ss_viewer/model/entry_info.h"

#include <gtest/gtest.h>

using namespace ssv;

// ── Helpers ─────────────────────────────────────────────────────────────

static entry_info make_root() {
    entry_info root;
    root.name = "Root Entry";
    root.type = stout::entry_type::root;
    root.full_path = "Root Entry";
    root.children_loaded = true;

    entry_info storage;
    storage.name = "Storage1";
    storage.type = stout::entry_type::storage;
    storage.full_path = "Root Entry/Storage1";
    storage.children_loaded = true;

    entry_info stream_a;
    stream_a.name = "StreamA";
    stream_a.type = stout::entry_type::stream;
    stream_a.size = 256;
    stream_a.full_path = "Root Entry/Storage1/StreamA";

    entry_info stream_b;
    stream_b.name = "\x05SummaryInformation";
    stream_b.type = stout::entry_type::stream;
    stream_b.size = 128;
    stream_b.full_path = "Root Entry/\x05SummaryInformation";

    storage.children.push_back(std::move(stream_a));
    root.children.push_back(std::move(storage));
    root.children.push_back(std::move(stream_b));
    return root;
}

// ── tree_label ──────────────────────────────────────────────────────────

TEST(EntryInfo, TreeLabelRoot) {
    entry_info ei;
    ei.name = "Root Entry";
    ei.type = stout::entry_type::root;
    EXPECT_EQ(tree_label(ei), "[+] Root Entry");
}

TEST(EntryInfo, TreeLabelStorage) {
    entry_info ei;
    ei.name = "MyStorage";
    ei.type = stout::entry_type::storage;
    EXPECT_EQ(tree_label(ei), "[+] MyStorage");
}

TEST(EntryInfo, TreeLabelStream) {
    entry_info ei;
    ei.name = "data.bin";
    ei.type = stout::entry_type::stream;
    EXPECT_EQ(tree_label(ei), "[F] data.bin");
}

TEST(EntryInfo, TreeLabelPropertyStream) {
    entry_info ei;
    ei.name = "\x05SummaryInformation";
    ei.type = stout::entry_type::stream;
    EXPECT_EQ(tree_label(ei), "[P] \x05SummaryInformation");
}

// ── to_tree_node ────────────────────────────────────────────────────────

TEST(EntryInfo, ToTreeNodePreservesStructure) {
    auto root = make_root();
    auto node = to_tree_node(root);

    EXPECT_EQ(node.label, "[+] Root Entry");
    ASSERT_EQ(node.children.size(), 2u);
    EXPECT_EQ(node.children[0].label, "[+] Storage1");
    ASSERT_EQ(node.children[0].children.size(), 1u);
    EXPECT_EQ(node.children[0].children[0].label, "[F] StreamA");
    EXPECT_EQ(node.children[1].label, "[P] \x05SummaryInformation");
}

TEST(EntryInfo, ToTreeNodeLeafHasNoChildren) {
    entry_info ei;
    ei.name = "leaf";
    ei.type = stout::entry_type::stream;
    auto node = to_tree_node(ei);
    EXPECT_TRUE(node.children.empty());
}

// ── flatten_paths ───────────────────────────────────────────────────────

TEST(EntryInfo, FlattenPathsCollapsedOnlyRoot) {
    auto root = make_root();
    std::unordered_set<std::string> expanded;
    std::vector<std::string> paths;
    flatten_paths(root, paths, expanded);

    // Nothing expanded → only root
    ASSERT_EQ(paths.size(), 1u);
    EXPECT_EQ(paths[0], "Root Entry");
}

TEST(EntryInfo, FlattenPathsExpandedRoot) {
    auto root = make_root();
    std::unordered_set<std::string> expanded;
    expanded.insert("[+] Root Entry");
    std::vector<std::string> paths;
    flatten_paths(root, paths, expanded);

    // Root + Storage1 + \x05SummaryInformation
    ASSERT_EQ(paths.size(), 3u);
    EXPECT_EQ(paths[0], "Root Entry");
    EXPECT_EQ(paths[1], "Root Entry/Storage1");
    EXPECT_EQ(paths[2], "Root Entry/\x05SummaryInformation");
}

TEST(EntryInfo, FlattenPathsFullyExpanded) {
    auto root = make_root();
    std::unordered_set<std::string> expanded;
    expanded.insert("[+] Root Entry");
    expanded.insert("[+] Storage1");
    std::vector<std::string> paths;
    flatten_paths(root, paths, expanded);

    // Root + Storage1 + StreamA + \x05SummaryInformation
    ASSERT_EQ(paths.size(), 4u);
    EXPECT_EQ(paths[0], "Root Entry");
    EXPECT_EQ(paths[1], "Root Entry/Storage1");
    EXPECT_EQ(paths[2], "Root Entry/Storage1/StreamA");
    EXPECT_EQ(paths[3], "Root Entry/\x05SummaryInformation");
}

// ── find_entry ──────────────────────────────────────────────────────────

TEST(EntryInfo, FindEntryRoot) {
    auto root = make_root();
    auto *found = find_entry(root, "Root Entry");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "Root Entry");
}

TEST(EntryInfo, FindEntryDeep) {
    auto root = make_root();
    auto *found = find_entry(root, "Root Entry/Storage1/StreamA");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "StreamA");
    EXPECT_EQ(found->size, 256u);
}

TEST(EntryInfo, FindEntryNotFound) {
    auto root = make_root();
    auto *found = find_entry(root, "Root Entry/NonExistent");
    EXPECT_EQ(found, nullptr);
}

TEST(EntryInfo, FindEntryPropertyStream) {
    auto root = make_root();
    auto *found = find_entry(root, "Root Entry/\x05SummaryInformation");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->size, 128u);
}

// ── expand_all ──────────────────────────────────────────────────────────

TEST(EntryInfo, ExpandAllAddsAllStorages) {
    auto root = make_root();
    std::unordered_set<std::string> expanded;
    expand_all(root, expanded);

    EXPECT_TRUE(expanded.count("[+] Root Entry"));
    EXPECT_TRUE(expanded.count("[+] Storage1"));
    EXPECT_EQ(expanded.size(), 2u); // only storages, not streams
}

TEST(EntryInfo, ExpandAllThenFlattenGivesAll) {
    auto root = make_root();
    std::unordered_set<std::string> expanded;
    expand_all(root, expanded);

    std::vector<std::string> paths;
    flatten_paths(root, paths, expanded);
    EXPECT_EQ(paths.size(), 4u);
}
