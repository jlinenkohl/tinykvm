#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tkvm_machine tkvm_machine_t;

enum {
	TKVM_OK = 0,
	TKVM_ERROR = -1,
	TKVM_INVALID_ARGUMENT = -2,
	TKVM_ERR_TIMEOUT = -10,
	TKVM_ERR_MEMORY = -11,
	TKVM_ERR_SYMBOL_NOT_FOUND = -12,
	TKVM_ERR_INVALID_STATE = -13,
	TKVM_ERR_MACHINE = -14,
};

enum {
	TKVM_SNAPSHOT_DISABLED = 0,
	TKVM_SNAPSHOT_OPEN = 1,
	TKVM_SNAPSHOT_CREATE = 2,
	TKVM_SNAPSHOT_OPEN_OR_CREATE = 3,
};

struct tkvm_options {
	uint64_t max_mem;
	uint32_t max_cow_mem;
	uint32_t stack_size;
	uint32_t reset_free_work_mem;
	uint64_t dylink_address_hint;
	uint64_t heap_address_hint;
	uint64_t vmem_base_address;
	int verbose_loader;
	int short_lived;
	int hugepages;
	int transparent_hugepages;
	int master_direct_memory_writes;
	int split_hugepages;
	int split_all_hugepages_during_loading;
	int allow_reset_to_new_master;
	int reset_copy_all_registers;
	int reset_enter_usermode;
	int reset_keep_all_work_memory;
	int relocate_fixed_mmap;
	int executable_heap;
	int mmap_backed_files;
	const char* snapshot_file;
	int snapshot_mode;
	size_t hugepages_arena_size;
};

int tkvm_init(int unsafe_syscalls);
void tkvm_options_set_defaults(struct tkvm_options* options);

int tkvm_machine_create(const uint8_t* binary, size_t binary_size,
	const struct tkvm_options* options, tkvm_machine_t** out_machine);
void tkvm_machine_destroy(tkvm_machine_t* machine);

int tkvm_machine_setup_linux(tkvm_machine_t* machine,
	const char* const* argv, size_t argc,
	const char* const* env, size_t envc);
int tkvm_machine_run(tkvm_machine_t* machine, float timeout_secs);
int tkvm_machine_return_value(tkvm_machine_t* machine, long* out_value);
int tkvm_machine_copy_to_guest(tkvm_machine_t* machine, uint64_t guest_addr, const void* data, size_t len);
int tkvm_machine_copy_from_guest(tkvm_machine_t* machine, void* dst, uint64_t guest_addr, size_t len);
int tkvm_machine_address_of(tkvm_machine_t* machine, const char* symbol, uint64_t* out_addr);
int tkvm_machine_vmcall0(tkvm_machine_t* machine, const char* symbol);
int tkvm_machine_vmcall1_u64(tkvm_machine_t* machine, const char* symbol, uint64_t arg0);
int tkvm_machine_vmcall_u64(tkvm_machine_t* machine, const char* symbol, const uint64_t* args, size_t argc);
int tkvm_machine_vmcall_addr_u64(tkvm_machine_t* machine, uint64_t addr, const uint64_t* args, size_t argc);
int tkvm_machine_timed_vmcall0(tkvm_machine_t* machine, const char* symbol, float timeout_secs);
int tkvm_machine_prepare_copy_on_write(tkvm_machine_t* machine, size_t max_work_mem);
int tkvm_machine_fork(const tkvm_machine_t* master, const struct tkvm_options* options, tkvm_machine_t** out_machine);
int tkvm_machine_reset_to(tkvm_machine_t* machine, const tkvm_machine_t* master,
	const struct tkvm_options* options, int* out_full_reset);

const char* tkvm_last_error(void);

#ifdef __cplusplus
}
#endif
