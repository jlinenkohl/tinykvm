#include <catch2/catch_test_macros.hpp>

#include <tinykvm/machine.hpp>
#include <elf.h>
#include <stdexcept>
#include <cstring>

extern std::vector<uint8_t> load_file(const std::string& filename);
extern std::pair<
	std::string,
	std::vector<uint8_t>
> build_and_load_nonstatic(const std::string& code, const std::string& args);

static const uint64_t MAX_MEMORY = 8ul << 20; /* 8MB */
static const std::vector<uint8_t> ld_linux_x86_64_so
	= load_file("/lib64/ld-linux-x86-64.so.2");

static const Elf64_Shdr* section_by_name_const(const std::vector<uint8_t>& elf, const char* name)
{
	if (elf.size() < sizeof(Elf64_Ehdr)) {
		throw std::runtime_error("ELF too small for header");
	}
	auto* ehdr = reinterpret_cast<const Elf64_Ehdr*>(elf.data());
	if (ehdr->e_shoff + ehdr->e_shnum * sizeof(Elf64_Shdr) > elf.size()) {
		throw std::runtime_error("ELF section table outside binary");
	}
	auto* shdr = reinterpret_cast<const Elf64_Shdr*>(elf.data() + ehdr->e_shoff);
	if (ehdr->e_shstrndx >= ehdr->e_shnum) {
		throw std::runtime_error("Invalid ELF shstrndx");
	}
	const auto& shstrtab = shdr[ehdr->e_shstrndx];
	if (shstrtab.sh_offset + shstrtab.sh_size > elf.size()) {
		throw std::runtime_error("ELF shstrtab outside binary");
	}
	const char* strings = reinterpret_cast<const char*>(elf.data() + shstrtab.sh_offset);
	for (uint16_t i = 0; i < ehdr->e_shnum; i++)
	{
		const char* shname = strings + shdr[i].sh_name;
		if (strcmp(shname, name) == 0) {
			return &shdr[i];
		}
	}
	return nullptr;
}

static uint64_t load_u64_from_elf_vaddr(const std::vector<uint8_t>& elf, uint64_t virt)
{
	if (elf.size() < sizeof(Elf64_Ehdr)) {
		throw std::runtime_error("ELF too small for header");
	}
	const auto* ehdr = reinterpret_cast<const Elf64_Ehdr*>(elf.data());
	if (ehdr->e_phoff + ehdr->e_phnum * sizeof(Elf64_Phdr) > elf.size()) {
		throw std::runtime_error("ELF program table outside binary");
	}

	const auto* phdr = reinterpret_cast<const Elf64_Phdr*>(elf.data() + ehdr->e_phoff);
	for (uint16_t i = 0; i < ehdr->e_phnum; i++)
	{
		if (phdr[i].p_type != PT_LOAD) continue;
		if (virt < phdr[i].p_vaddr) continue;

		const uint64_t rel = virt - phdr[i].p_vaddr;
		if (rel + sizeof(uint64_t) > phdr[i].p_filesz) continue;

		const uint64_t file_off = phdr[i].p_offset + rel;
		if (file_off + sizeof(uint64_t) > elf.size()) {
			throw std::runtime_error("ELF virtual address maps outside file contents");
		}

		uint64_t value = 0;
		std::memcpy(&value, elf.data() + file_off, sizeof(value));
		return value;
	}

	throw std::runtime_error("ELF virtual address not backed by PT_LOAD file data");
}

static uint64_t find_first_relr_direct_target(const std::vector<uint8_t>& elf)
{
	const auto* relr = section_by_name_const(elf, ".relr.dyn");
	if (relr == nullptr) {
		throw std::runtime_error("ELF is missing .relr.dyn section");
	}
	if ((relr->sh_size % sizeof(Elf64_Addr)) != 0) {
		throw std::runtime_error("Malformed .relr.dyn section size");
	}
	if (relr->sh_offset + relr->sh_size > elf.size()) {
		throw std::runtime_error("ELF .relr.dyn payload outside binary");
	}

	const size_t relr_ents = relr->sh_size / sizeof(Elf64_Addr);
	auto* relr_addr = reinterpret_cast<const Elf64_Addr*>(elf.data() + relr->sh_offset);
	for (size_t i = 0; i < relr_ents; i++)
	{
		if ((relr_addr[i] & 1ULL) == 0) {
			return relr_addr[i];
		}
	}

	throw std::runtime_error("ELF .relr.dyn has no direct relocation entry");
}

static Elf64_Rela find_first_relative_rela(const std::vector<uint8_t>& elf)
{
	const auto* rela = section_by_name_const(elf, ".rela.dyn");
	if (rela == nullptr) {
		throw std::runtime_error("ELF is missing .rela.dyn section");
	}
	if ((rela->sh_size % sizeof(Elf64_Rela)) != 0) {
		throw std::runtime_error("Malformed .rela.dyn section size");
	}
	if (rela->sh_offset + rela->sh_size > elf.size()) {
		throw std::runtime_error("ELF .rela.dyn payload outside binary");
	}

	const size_t rela_ents = rela->sh_size / sizeof(Elf64_Rela);
	auto* rela_addr = reinterpret_cast<const Elf64_Rela*>(elf.data() + rela->sh_offset);
	for (size_t i = 0; i < rela_ents; i++)
	{
		if (ELF64_R_TYPE(rela_addr[i].r_info) == R_X86_64_RELATIVE) {
			return rela_addr[i];
		}
	}

	throw std::runtime_error("ELF .rela.dyn has no R_X86_64_RELATIVE entry");
}

TEST_CASE("RELR direct relocation writes expected value", "[ELF][reloc]")
{
	const uint64_t relr_target = find_first_relr_direct_target(ld_linux_x86_64_so);
	const uint64_t original_value = load_u64_from_elf_vaddr(ld_linux_x86_64_so, relr_target);

	tinykvm::Machine machine { ld_linux_x86_64_so, {
		.max_mem = MAX_MEMORY,
		.executable_heap = true,
		.irelative_mode = tinykvm::MachineOptions::IRelativeMode::BestEffort,
		.mmap_backed_files = true,
	} };

	const uint64_t target_addr = machine.image_base() + relr_target;
	uint64_t relocated_value = 0;
	machine.copy_from_guest(&relocated_value, target_addr, sizeof(relocated_value));

	const uint64_t expected = original_value + machine.image_base();
	REQUIRE(relocated_value == expected);
}

TEST_CASE("RELA relative relocation writes expected value", "[ELF][reloc]")
{
	const auto [fixture_path, fixture] = build_and_load_nonstatic(R"M(
int global = 7;
int* gp = &global;
int exported(void) { return *gp; }
)M", "-shared -fPIC");
	(void) fixture_path;
	const Elf64_Rela rel = find_first_relative_rela(fixture);

	tinykvm::Machine machine { fixture, {
		.max_mem = MAX_MEMORY,
		.executable_heap = true,
		.irelative_mode = tinykvm::MachineOptions::IRelativeMode::BestEffort,
		.mmap_backed_files = true,
	} };

	const uint64_t target_addr = machine.image_base() + rel.r_offset;
	uint64_t relocated_value = 0;
	machine.copy_from_guest(&relocated_value, target_addr, sizeof(relocated_value));

	const uint64_t expected = machine.image_base() + static_cast<uint64_t>(rel.r_addend);
	REQUIRE(relocated_value == expected);
}
