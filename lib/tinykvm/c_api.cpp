#include "c_api.h"

#include "machine.hpp"

#include <cstring>
#include <exception>
#include <new>
#include <string>
#include <string_view>
#include <vector>

using tinykvm::Machine;
using tinykvm::MachineOptions;

struct tkvm_machine {
	std::vector<uint8_t> owned_binary;
	Machine* impl = nullptr;
};

namespace {
thread_local std::string g_last_error;

int set_error(const char* msg, int code = TKVM_ERROR)
{
	g_last_error = msg != nullptr ? msg : "Unknown error";
	return code;
}

int set_exception_error(const std::exception& e)
{
	if (dynamic_cast<const tinykvm::MachineTimeoutException*>(&e) != nullptr) {
		return set_error(e.what(), TKVM_ERR_TIMEOUT);
	}
	if (dynamic_cast<const tinykvm::MemoryException*>(&e) != nullptr) {
		return set_error(e.what(), TKVM_ERR_MEMORY);
	}
	if (const auto* me = dynamic_cast<const tinykvm::MachineException*>(&e); me != nullptr) {
		const char* msg = me->what();
		if (msg != nullptr && (strstr(msg, "not prepared") != nullptr || strstr(msg, "forked") != nullptr)) {
			return set_error(msg, TKVM_ERR_INVALID_STATE);
		}
		return set_error(msg, TKVM_ERR_MACHINE);
	}
	if (dynamic_cast<const std::bad_alloc*>(&e) != nullptr) {
		return set_error(e.what(), TKVM_ERR_MEMORY);
	}
	return set_error(e.what(), TKVM_ERROR);
}

MachineOptions convert_options(const tkvm_options* options)
{
	MachineOptions converted {};
	if (options == nullptr) {
		return converted;
	}

	converted.max_mem = options->max_mem;
	converted.max_cow_mem = options->max_cow_mem;
	converted.stack_size = options->stack_size;
	converted.reset_free_work_mem = options->reset_free_work_mem;
	converted.dylink_address_hint = options->dylink_address_hint;
	converted.heap_address_hint = options->heap_address_hint;
	converted.vmem_base_address = options->vmem_base_address;
	converted.verbose_loader = options->verbose_loader != 0;
	converted.short_lived = options->short_lived != 0;
	converted.hugepages = options->hugepages != 0;
	converted.transparent_hugepages = options->transparent_hugepages != 0;
	converted.master_direct_memory_writes = options->master_direct_memory_writes != 0;
	converted.split_hugepages = options->split_hugepages != 0;
	converted.split_all_hugepages_during_loading = options->split_all_hugepages_during_loading != 0;
	converted.allow_reset_to_new_master = options->allow_reset_to_new_master != 0;
	converted.reset_copy_all_registers = options->reset_copy_all_registers != 0;
	converted.reset_enter_usermode = options->reset_enter_usermode != 0;
	converted.reset_keep_all_work_memory = options->reset_keep_all_work_memory != 0;
	converted.relocate_fixed_mmap = options->relocate_fixed_mmap != 0;
	converted.executable_heap = options->executable_heap != 0;
	converted.mmap_backed_files = options->mmap_backed_files != 0;
	converted.snapshot_mode = static_cast<MachineOptions::SnapshotMode>(options->snapshot_mode);
	converted.hugepages_arena_size = options->hugepages_arena_size;
	if (options->snapshot_file != nullptr) {
		converted.snapshot_file = options->snapshot_file;
	}
	return converted;
}

std::vector<std::string> convert_strv(const char* const* data, size_t count)
{
	std::vector<std::string> out;
	out.reserve(count);
	for (size_t i = 0; i < count; i++) {
		out.emplace_back(data[i] != nullptr ? data[i] : "");
	}
	return out;
}

int vmcall_addr_u64_impl(tkvm_machine_t* machine, uint64_t addr, const uint64_t* args, size_t argc)
{
	if (machine == nullptr || machine->impl == nullptr || addr == 0 || (argc > 0 && args == nullptr)) {
		return set_error("Invalid argument in vmcall_addr_u64_impl", TKVM_INVALID_ARGUMENT);
	}
	if (argc > 6) {
		return set_error("vmcall supports at most 6 u64 arguments", TKVM_INVALID_ARGUMENT);
	}

	try {
		switch (argc) {
		case 0: machine->impl->vmcall(addr); break;
		case 1: machine->impl->vmcall(addr, args[0]); break;
		case 2: machine->impl->vmcall(addr, args[0], args[1]); break;
		case 3: machine->impl->vmcall(addr, args[0], args[1], args[2]); break;
		case 4: machine->impl->vmcall(addr, args[0], args[1], args[2], args[3]); break;
		case 5: machine->impl->vmcall(addr, args[0], args[1], args[2], args[3], args[4]); break;
		case 6: machine->impl->vmcall(addr, args[0], args[1], args[2], args[3], args[4], args[5]); break;
		default: return set_error("vmcall argument dispatch failed", TKVM_INVALID_ARGUMENT);
		}
		return TKVM_OK;
	} catch (const std::exception& e) {
		return set_exception_error(e);
	} catch (...) {
		return set_error("vmcall_addr_u64_impl failed with unknown exception");
	}
}
} // namespace

extern "C" {

int tkvm_init(int unsafe_syscalls)
{
	try {
		Machine::init();
		Machine::setup_linux_system_calls(unsafe_syscalls != 0);
		return TKVM_OK;
	} catch (const std::exception& e) {
		return set_exception_error(e);
	} catch (...) {
		return set_error("tkvm_init failed with unknown exception");
	}
}

void tkvm_options_set_defaults(struct tkvm_options* options)
{
	if (options == nullptr) {
		return;
	}
	MachineOptions defaults {};
	options->max_mem = defaults.max_mem;
	options->max_cow_mem = defaults.max_cow_mem;
	options->stack_size = defaults.stack_size;
	options->reset_free_work_mem = defaults.reset_free_work_mem;
	options->dylink_address_hint = defaults.dylink_address_hint;
	options->heap_address_hint = defaults.heap_address_hint;
	options->vmem_base_address = defaults.vmem_base_address;
	options->verbose_loader = defaults.verbose_loader;
	options->short_lived = defaults.short_lived;
	options->hugepages = defaults.hugepages;
	options->transparent_hugepages = defaults.transparent_hugepages;
	options->master_direct_memory_writes = defaults.master_direct_memory_writes;
	options->split_hugepages = defaults.split_hugepages;
	options->split_all_hugepages_during_loading = defaults.split_all_hugepages_during_loading;
	options->allow_reset_to_new_master = defaults.allow_reset_to_new_master;
	options->reset_copy_all_registers = defaults.reset_copy_all_registers;
	options->reset_enter_usermode = defaults.reset_enter_usermode;
	options->reset_keep_all_work_memory = defaults.reset_keep_all_work_memory;
	options->relocate_fixed_mmap = defaults.relocate_fixed_mmap;
	options->executable_heap = defaults.executable_heap;
	options->mmap_backed_files = defaults.mmap_backed_files;
	options->snapshot_file = nullptr;
	options->snapshot_mode = defaults.snapshot_mode;
	options->hugepages_arena_size = defaults.hugepages_arena_size;
}

int tkvm_machine_create(const uint8_t* binary, size_t binary_size,
	const struct tkvm_options* options, tkvm_machine_t** out_machine)
{
	if (binary == nullptr || binary_size == 0 || out_machine == nullptr) {
		return set_error("Invalid arguments to tkvm_machine_create", TKVM_INVALID_ARGUMENT);
	}

	try {
		MachineOptions converted = convert_options(options);
		tkvm_machine* machine = new tkvm_machine;
		machine->owned_binary.assign(binary, binary + binary_size);
		machine->impl = new Machine(machine->owned_binary, converted);
		*out_machine = machine;
		return TKVM_OK;
	} catch (const std::exception& e) {
		return set_exception_error(e);
	} catch (...) {
		return set_error("tkvm_machine_create failed with unknown exception");
	}
}

void tkvm_machine_destroy(tkvm_machine_t* machine)
{
	if (machine == nullptr) {
		return;
	}
	delete machine->impl;
	delete machine;
}

int tkvm_machine_setup_linux(tkvm_machine_t* machine,
	const char* const* argv, size_t argc,
	const char* const* env, size_t envc)
{
	if (machine == nullptr || machine->impl == nullptr) {
		return set_error("Invalid machine in tkvm_machine_setup_linux", TKVM_INVALID_ARGUMENT);
	}
	if ((argc > 0 && argv == nullptr) || (envc > 0 && env == nullptr)) {
		return set_error("Invalid argv/env arguments in tkvm_machine_setup_linux", TKVM_INVALID_ARGUMENT);
	}

	try {
		auto argv_vec = convert_strv(argv, argc);
		auto env_vec = convert_strv(env, envc);
		machine->impl->setup_linux(argv_vec, env_vec);
		return TKVM_OK;
	} catch (const std::exception& e) {
		return set_exception_error(e);
	} catch (...) {
		return set_error("tkvm_machine_setup_linux failed with unknown exception");
	}
}

int tkvm_machine_run(tkvm_machine_t* machine, float timeout_secs)
{
	if (machine == nullptr || machine->impl == nullptr) {
		return set_error("Invalid machine in tkvm_machine_run", TKVM_INVALID_ARGUMENT);
	}

	try {
		machine->impl->run(timeout_secs);
		return TKVM_OK;
	} catch (const std::exception& e) {
		return set_exception_error(e);
	} catch (...) {
		return set_error("tkvm_machine_run failed with unknown exception");
	}
}

int tkvm_machine_return_value(tkvm_machine_t* machine, long* out_value)
{
	if (machine == nullptr || machine->impl == nullptr || out_value == nullptr) {
		return set_error("Invalid argument in tkvm_machine_return_value", TKVM_INVALID_ARGUMENT);
	}

	try {
		*out_value = machine->impl->return_value();
		return TKVM_OK;
	} catch (const std::exception& e) {
		return set_exception_error(e);
	} catch (...) {
		return set_error("tkvm_machine_return_value failed with unknown exception");
	}
}

int tkvm_machine_copy_to_guest(tkvm_machine_t* machine, uint64_t guest_addr, const void* data, size_t len)
{
	if (machine == nullptr || machine->impl == nullptr || (len > 0 && data == nullptr)) {
		return set_error("Invalid argument in tkvm_machine_copy_to_guest", TKVM_INVALID_ARGUMENT);
	}

	try {
		machine->impl->copy_to_guest(guest_addr, data, len, false);
		return TKVM_OK;
	} catch (const std::exception& e) {
		return set_exception_error(e);
	} catch (...) {
		return set_error("tkvm_machine_copy_to_guest failed with unknown exception");
	}
}

int tkvm_machine_copy_from_guest(tkvm_machine_t* machine, void* dst, uint64_t guest_addr, size_t len)
{
	if (machine == nullptr || machine->impl == nullptr || (len > 0 && dst == nullptr)) {
		return set_error("Invalid argument in tkvm_machine_copy_from_guest", TKVM_INVALID_ARGUMENT);
	}

	try {
		machine->impl->copy_from_guest(dst, guest_addr, len);
		return TKVM_OK;
	} catch (const std::exception& e) {
		return set_exception_error(e);
	} catch (...) {
		return set_error("tkvm_machine_copy_from_guest failed with unknown exception");
	}
}

int tkvm_machine_address_of(tkvm_machine_t* machine, const char* symbol, uint64_t* out_addr)
{
	if (machine == nullptr || machine->impl == nullptr || symbol == nullptr || out_addr == nullptr) {
		return set_error("Invalid argument in tkvm_machine_address_of", TKVM_INVALID_ARGUMENT);
	}

	try {
		*out_addr = machine->impl->address_of(symbol);
		if (*out_addr == 0) {
			return set_error("Symbol not found", TKVM_ERR_SYMBOL_NOT_FOUND);
		}
		return TKVM_OK;
	} catch (const std::exception& e) {
		return set_exception_error(e);
	} catch (...) {
		return set_error("tkvm_machine_address_of failed with unknown exception");
	}
}

int tkvm_machine_vmcall0(tkvm_machine_t* machine, const char* symbol)
{
	if (machine == nullptr || machine->impl == nullptr || symbol == nullptr) {
		return set_error("Invalid argument in tkvm_machine_vmcall0", TKVM_INVALID_ARGUMENT);
	}

	try {
		const auto addr = machine->impl->address_of(symbol);
		if (addr == 0) {
			return set_error("Symbol not found in tkvm_machine_vmcall0", TKVM_ERR_SYMBOL_NOT_FOUND);
		}
		machine->impl->vmcall(addr);
		return TKVM_OK;
	} catch (const std::exception& e) {
		return set_exception_error(e);
	} catch (...) {
		return set_error("tkvm_machine_vmcall0 failed with unknown exception");
	}
}

int tkvm_machine_vmcall1_u64(tkvm_machine_t* machine, const char* symbol, uint64_t arg0)
{
	if (machine == nullptr || machine->impl == nullptr || symbol == nullptr) {
		return set_error("Invalid argument in tkvm_machine_vmcall1_u64", TKVM_INVALID_ARGUMENT);
	}

	try {
		const auto addr = machine->impl->address_of(symbol);
		if (addr == 0) {
			return set_error("Symbol not found in tkvm_machine_vmcall1_u64", TKVM_ERR_SYMBOL_NOT_FOUND);
		}
		machine->impl->vmcall(addr, arg0);
		return TKVM_OK;
	} catch (const std::exception& e) {
		return set_exception_error(e);
	} catch (...) {
		return set_error("tkvm_machine_vmcall1_u64 failed with unknown exception");
	}
}

int tkvm_machine_vmcall_u64(tkvm_machine_t* machine, const char* symbol, const uint64_t* args, size_t argc)
{
	if (machine == nullptr || machine->impl == nullptr || symbol == nullptr) {
		return set_error("Invalid argument in tkvm_machine_vmcall_u64", TKVM_INVALID_ARGUMENT);
	}

	try {
		const auto addr = machine->impl->address_of(symbol);
		if (addr == 0) {
			return set_error("Symbol not found in tkvm_machine_vmcall_u64", TKVM_ERR_SYMBOL_NOT_FOUND);
		}
		return vmcall_addr_u64_impl(machine, addr, args, argc);
	} catch (const std::exception& e) {
		return set_exception_error(e);
	} catch (...) {
		return set_error("tkvm_machine_vmcall_u64 failed with unknown exception");
	}
}

int tkvm_machine_vmcall_addr_u64(tkvm_machine_t* machine, uint64_t addr, const uint64_t* args, size_t argc)
{
	return vmcall_addr_u64_impl(machine, addr, args, argc);
}

int tkvm_machine_timed_vmcall0(tkvm_machine_t* machine, const char* symbol, float timeout_secs)
{
	if (machine == nullptr || machine->impl == nullptr || symbol == nullptr) {
		return set_error("Invalid argument in tkvm_machine_timed_vmcall0", TKVM_INVALID_ARGUMENT);
	}

	try {
		const auto addr = machine->impl->address_of(symbol);
		if (addr == 0) {
			return set_error("Symbol not found in tkvm_machine_timed_vmcall0", TKVM_ERR_SYMBOL_NOT_FOUND);
		}
		machine->impl->timed_vmcall(addr, timeout_secs);
		return TKVM_OK;
	} catch (const std::exception& e) {
		return set_exception_error(e);
	} catch (...) {
		return set_error("tkvm_machine_timed_vmcall0 failed with unknown exception");
	}
}

int tkvm_machine_prepare_copy_on_write(tkvm_machine_t* machine, size_t max_work_mem)
{
	if (machine == nullptr || machine->impl == nullptr) {
		return set_error("Invalid machine in tkvm_machine_prepare_copy_on_write", TKVM_INVALID_ARGUMENT);
	}

	try {
		machine->impl->prepare_copy_on_write(max_work_mem);
		return TKVM_OK;
	} catch (const std::exception& e) {
		return set_exception_error(e);
	} catch (...) {
		return set_error("tkvm_machine_prepare_copy_on_write failed with unknown exception");
	}
}

int tkvm_machine_fork(const tkvm_machine_t* master, const struct tkvm_options* options, tkvm_machine_t** out_machine)
{
	if (master == nullptr || master->impl == nullptr || out_machine == nullptr) {
		return set_error("Invalid argument in tkvm_machine_fork", TKVM_INVALID_ARGUMENT);
	}

	try {
		MachineOptions converted = convert_options(options);
		tkvm_machine* forked = new tkvm_machine;
		forked->impl = new Machine(*master->impl, converted);
		*out_machine = forked;
		return TKVM_OK;
	} catch (const std::exception& e) {
		return set_exception_error(e);
	} catch (...) {
		return set_error("tkvm_machine_fork failed with unknown exception");
	}
}

int tkvm_machine_reset_to(tkvm_machine_t* machine, const tkvm_machine_t* master,
	const struct tkvm_options* options, int* out_full_reset)
{
	if (machine == nullptr || machine->impl == nullptr || master == nullptr || master->impl == nullptr) {
		return set_error("Invalid argument in tkvm_machine_reset_to", TKVM_INVALID_ARGUMENT);
	}

	try {
		MachineOptions converted = convert_options(options);
		const bool full_reset = machine->impl->reset_to(*master->impl, converted);
		if (out_full_reset != nullptr) {
			*out_full_reset = full_reset ? 1 : 0;
		}
		return TKVM_OK;
	} catch (const std::exception& e) {
		return set_exception_error(e);
	} catch (...) {
		return set_error("tkvm_machine_reset_to failed with unknown exception");
	}
}

const char* tkvm_last_error(void)
{
	return g_last_error.c_str();
}

} // extern "C"
