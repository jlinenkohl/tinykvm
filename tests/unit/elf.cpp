#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <elf.h>
#include <stdexcept>
#include <string_view>
#include <tinykvm/machine.hpp>
#include <tinykvm/rsp_client.hpp>
extern std::vector<uint8_t> load_file(const std::string& filename);
static const uint64_t MAX_MEMORY = 8ul << 20; /* 8MB */
static const std::vector<std::string> env{
	"LC_TYPE=C", "LC_ALL=C", "USER=root"};
static const std::vector<uint8_t> ld_linux_x86_64_so
	= load_file("/lib64/ld-linux-x86-64.so.2");

static const Elf64_Shdr* section_by_name(std::string_view binary, const char* name)
{
	if (binary.size() < sizeof(Elf64_Ehdr)) {
		throw std::runtime_error("ELF binary too short");
	}
	const auto* ehdr = (const Elf64_Ehdr*) binary.data();
	if (ehdr->e_shoff + sizeof(Elf64_Shdr) * ehdr->e_shnum > binary.size()) {
		throw std::runtime_error("ELF section headers out of bounds");
	}
	const auto* shdr = (const Elf64_Shdr*) (binary.data() + ehdr->e_shoff);
	if (ehdr->e_shstrndx >= ehdr->e_shnum) {
		throw std::runtime_error("Invalid ELF shstrndx");
	}
	const auto& shstrtab = shdr[ehdr->e_shstrndx];
	if (shstrtab.sh_offset + shstrtab.sh_size > binary.size()) {
		throw std::runtime_error("ELF shstrtab out of bounds");
	}
	const char* strings = binary.data() + shstrtab.sh_offset;

	for (int i = 0; i < ehdr->e_shnum; i++) {
		const char* shname = &strings[shdr[i].sh_name];
		if (strcmp(shname, name) == 0) {
			return &shdr[i];
		}
	}
	return nullptr;
}

static uint64_t first_relr_target_vaddr(std::string_view binary)
{
	const auto* relr = section_by_name(binary, ".relr.dyn");
	if (relr == nullptr) {
		throw std::runtime_error("Missing .relr.dyn section");
	}
	if ((relr->sh_size % sizeof(Elf64_Addr)) != 0) {
		throw std::runtime_error("Malformed .relr.dyn section");
	}
	if (relr->sh_offset + relr->sh_size > binary.size()) {
		throw std::runtime_error(".relr.dyn out of bounds");
	}

	const auto* relr_addr = (const Elf64_Addr*) (binary.data() + relr->sh_offset);
	const size_t relr_ents = relr->sh_size / sizeof(Elf64_Addr);
	for (size_t i = 0; i < relr_ents; i++) {
		if ((relr_addr[i] & 1ULL) == 0) {
			return relr_addr[i];
		}
	}
	throw std::runtime_error("No direct RELR relocation target found");
}

static uint64_t read_u64_from_binary_vaddr(std::string_view binary, uint64_t vaddr)
{
	const auto* ehdr = (const Elf64_Ehdr*) binary.data();
	if (ehdr->e_phoff + sizeof(Elf64_Phdr) * ehdr->e_phnum > binary.size()) {
		throw std::runtime_error("ELF program headers out of bounds");
	}
	const auto* phdr = (const Elf64_Phdr*) (binary.data() + ehdr->e_phoff);

	for (int i = 0; i < ehdr->e_phnum; i++) {
		if (phdr[i].p_type != PT_LOAD) {
			continue;
		}
		const uint64_t seg_begin = phdr[i].p_vaddr;
		const uint64_t seg_end = phdr[i].p_vaddr + phdr[i].p_filesz;
		if (vaddr < seg_begin || vaddr + sizeof(uint64_t) > seg_end) {
			continue;
		}

		const uint64_t in_seg = vaddr - seg_begin;
		const uint64_t file_ofs = phdr[i].p_offset + in_seg;
		if (file_ofs + sizeof(uint64_t) > binary.size()) {
			throw std::runtime_error("ELF vaddr maps outside file");
		}
		uint64_t value = 0;
		std::memcpy(&value, binary.data() + file_ofs, sizeof(value));
		return value;
	}

	throw std::runtime_error("ELF vaddr not file-backed by PT_LOAD");
}

static uint64_t read_u64_from_guest(const tinykvm::Machine& machine, uint64_t guest_addr)
{
	uint64_t value = 0;
	std::memcpy(&value, machine.main_memory().safely_at(guest_addr, sizeof(value)), sizeof(value));
	return value;
}

TEST_CASE("Initialize KVM", "[Initialize]")
{
	tinykvm::Machine::init();
}

TEST_CASE("Verify dynamic Rust ELF", "[ELF]")
{
	std::string guest_filename
		= std::string(get_current_dir_name()) + "/../unit/elf/rust.elf";
	// Make filename absolute
	char abs_path[PATH_MAX];
	realpath(guest_filename.c_str(), abs_path);
	guest_filename = abs_path;

	tinykvm::Machine machine { ld_linux_x86_64_so, {
		.max_mem = MAX_MEMORY,
		.verbose_loader = true,
		.executable_heap = true,
		.mmap_backed_files = true
	} };
	// Allow opening all files (for dynamic linker)
	machine.fds().set_open_readable_callback(
	[&] (std::string& path) -> bool {
		return true;
	});
	// Load the dynamic linker instead of the program
	std::vector<std::string> args;
	args.push_back("/lib64/ld-linux-x86-64.so.2");
	args.push_back(guest_filename);
	// We need to create a Linux environment for runtimes to work well
	machine.setup_linux_system_calls();
	machine.setup_linux(args, env);

	try {
		// Run for at most 4 seconds before giving up
		machine.run(4.0f);
	} catch (const std::exception& ex) {
		printf("Exception: %s\n", ex.what());
		if (getenv("GDB") != nullptr)
		{
			tinykvm::RSP server(guest_filename, machine, 2159);
			printf("Waiting 60s for remote GDB on port 2159...\n");
			auto client = server.accept(60);
			if (client) {
				printf("Now debugging rust.elf\n");
				while(client->process_one());
			}
		}
	}

	REQUIRE(machine.return_value() == 231);
}

TEST_CASE("Verify dynamic Rust ELF (himem)", "[ELF]")
{
	const uint64_t HIMEM = 128ULL << 30; /* 128GB */
	tinykvm::Machine machine{ld_linux_x86_64_so, {
		.max_mem = MAX_MEMORY,
		.dylink_address_hint = HIMEM + 0x200000,
		.vmem_base_address = HIMEM,
		.master_direct_memory_writes = true,
		.executable_heap = true,
		.mmap_backed_files = true
	}};
	// Use constrained working memory
	machine.prepare_copy_on_write(MAX_MEMORY);
	// Allow opening all files (for dynamic linker)
	machine.fds().set_open_readable_callback(
	[&] (std::string& path) -> bool {
		return true;
	});
	// Load the dynamic linker instead of the program
	std::vector<std::string> args;
	args.push_back("/lib64/ld-linux-x86-64.so.2");
	args.push_back(std::string(get_current_dir_name()) + "/../unit/elf/rust.elf");
	// We need to create a Linux environment for runtimes to work well
	machine.setup_linux(args, env);
	REQUIRE(machine.entry_address() > HIMEM);

	// Run for at most 4 seconds before giving up
	machine.run(4.0f);

	REQUIRE(machine.return_value() == 231);
}

TEST_CASE("Relocation ownership Auto vs GuestOnly", "[ELF][relocation]")
{
	constexpr uint64_t IMAGE_BASE = 0x300000;
	const std::string_view binary {
		reinterpret_cast<const char*>(ld_linux_x86_64_so.data()),
		ld_linux_x86_64_so.size()
	};

	const uint64_t relr_target = first_relr_target_vaddr(binary);
	const uint64_t expected_unrelocated = read_u64_from_binary_vaddr(binary, relr_target);
	const uint64_t target_guest_addr = IMAGE_BASE + relr_target;

	tinykvm::Machine guest_only{ld_linux_x86_64_so, {
		.max_mem = MAX_MEMORY,
		.dylink_address_hint = IMAGE_BASE,
		.relocation_ownership_mode = tinykvm::MachineOptions::RelocationOwnershipMode::GuestOnly,
	}};
	tinykvm::Machine auto_mode{ld_linux_x86_64_so, {
		.max_mem = MAX_MEMORY,
		.dylink_address_hint = IMAGE_BASE,
		.relocation_ownership_mode = tinykvm::MachineOptions::RelocationOwnershipMode::Auto,
	}};

	const uint64_t guest_only_value = read_u64_from_guest(guest_only, target_guest_addr);
	const uint64_t auto_mode_value = read_u64_from_guest(auto_mode, target_guest_addr);

	REQUIRE(guest_only_value == expected_unrelocated);
	REQUIRE(auto_mode_value == expected_unrelocated + IMAGE_BASE);
}
