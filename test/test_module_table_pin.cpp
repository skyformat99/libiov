/*
 * Copyright (c) 2016, PLUMgrid, http://plumgrid.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <memory>
#include <vector>

#include <linux/bpf.h>
#include <libiov.h>
#include "libiov/command.h"
#include "libiov/module.h"
#include "libiov/filesystem.h"
#include "libiov/metadata.h"
#include "libiov/table.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

using std::string;
using std::vector;
using std::unique_ptr;
using namespace iov;

TEST_CASE("test table loading and saving", "[module_table_pin]") {
  char *uuid_str = NULL;
  int fd;
  uint32_t key;
  FileSystem fs;
  Table table;
  IOModule module;
  MetaData meta;
  ebpf::BPFModule *bpf_mod;
  string path = ModulePath;
  std::ofstream tableFile;
  std::ofstream metaFile;
  string text = "struct host { u64 mac; int ifindex; int pad; }; struct packet { u64 rx_pkt; u64 tx_pkt; }; BPF_TABLE(\"hash\", struct host, struct packet, num_ports, 1);";

  tableFile.open("/var/tmp/table.txt");
  metaFile.open("/var/tmp/meta.txt");

  module.Init(std::move(text), IOModule::NET_POLICY);

  bpf_mod = module.GetBpfModule();
   
  fd = table.Insert(BPF_MAP_TYPE_HASH, bpf_mod->table_key_size(0), 
                    bpf_mod->table_leaf_size(0), 1);

  uuid_str = new char[100];
  fs.GenerateUuid(uuid_str);

  path.append(uuid_str);
  fs.CreatePath(path);

  path.append(StatePath);
  fs.CreatePath(path);

  REQUIRE(fs.Save(path.c_str(), bpf_mod->table_name(0), fd) == 0);

  string metadata = path; 
  metadata.append(MetadataPath); 
  fs.CreatePath(metadata);

  path.append(bpf_mod->table_name(0));

  tableFile << path.c_str();

  string table_key = bpf_mod->table_key_desc(0);
  string table_leaf = bpf_mod->table_leaf_desc(0);
  size_t key_size = bpf_mod->table_key_size(0);
  size_t leaf_size = bpf_mod->table_leaf_size(0);

  fd = table.Insert(BPF_MAP_TYPE_HASH, sizeof(uint32_t), sizeof(struct descr), 1);

  REQUIRE(fs.Save(metadata.c_str(), bpf_mod->table_name(0), fd) == 0);

  string key_leaf_path = metadata;
  metadata.append(bpf_mod->table_name(0));

  metaFile << metadata.c_str();

  meta.item.key_desc_size = table_key.length();
  meta.item.leaf_desc_size = table_leaf.length();
  meta.item.key_size = key_size;
  meta.item.leaf_size = leaf_size;

  key = 0;
  REQUIRE(table.Update(fd, &key, &meta.item, BPF_ANY) == 0);

  fd = table.Insert(BPF_MAP_TYPE_HASH, sizeof(uint32_t), table_key.length(), 1);

  string f_key_desc = bpf_mod->table_name(0);
  f_key_desc.append(KeyDesc);
  REQUIRE(fs.Save(key_leaf_path.c_str(), f_key_desc.c_str(), fd) == 0);

  REQUIRE(table.Update(fd, &key, (void *)table_key.c_str(), BPF_ANY) == 0);

  fd = table.Insert(BPF_MAP_TYPE_HASH, sizeof(uint32_t), table_leaf.length(), 1);

  string f_leaf_desc = bpf_mod->table_name(0);
  f_leaf_desc.append(LeafDesc);
  REQUIRE(fs.Save(key_leaf_path.c_str(), f_leaf_desc.c_str(), fd) == 0);

  REQUIRE(table.Update(fd, &key, (void *)table_leaf.c_str(), BPF_ANY) == 0);

  delete[] uuid_str;
  tableFile.close();
  metaFile.close();
}
