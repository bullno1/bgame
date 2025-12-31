#ifndef BGAME_UXN_H
#define BGAME_UXN_H

#include <buxn/vm/vm.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "asset/binary.h"
#include "str.h"

#if defined(__linux__) && BGAME_RELOADABLE
#	define BGAME_UXN_DEBUGGABLE 1
#else
#	define BGAME_UXN_DEBUGGABLE 0
#endif

typedef struct bgame_uxn_dbg_s bgame_uxn_dbg_t;

typedef struct bgame_script_instance_s {
	uint8_t zero_page[256];
	uint8_t device[256];
	bgame_binary_t* rom;
} bgame_script_instance_t;

typedef enum {
	BGAME_UXN_DBG_DETACH,
	BGAME_UXN_DBG_ATTACH,
	BGAME_UXN_DBG_BREAK,
} bgame_uxn_dbg_op_t;

void
bgame_handle_uxn_dbg(buxn_vm_t* vm, bgame_uxn_dbg_t** dbgp, bgame_uxn_dbg_op_t op);

static inline void
bgame_unpack_script_instance(
	const bgame_script_instance_t* instance,
	buxn_vm_t* vm
) {
	memcpy(vm->memory, instance->zero_page, sizeof(instance->zero_page));
	memcpy(vm->device, instance->device, sizeof(instance->device));
	memcpy(vm->memory + BUXN_RESET_VECTOR, instance->rom->data, instance->rom->size);
}

static inline void
bgame_pack_script_instance(
	bgame_script_instance_t* script,
	const buxn_vm_t* vm
) {
	memcpy(script->zero_page, vm->memory, sizeof(script->zero_page));
	memcpy(script->device, vm->device, sizeof(script->device));
}

static inline bgame_str_t
bgame_read_vm_str(const buxn_vm_t* vm, uint16_t addr) {
	if (addr == 0) { return (bgame_str_t){ 0 }; }

	uint16_t end_addr = buxn_vm_mem_load2(vm, addr);
	uint16_t start_addr = addr + 2;
	if (end_addr >= start_addr) {
		return (bgame_str_t){
			.chars = (const char*)&vm->memory[start_addr],
			.len = (int)(end_addr - start_addr),
		};
	} else {
		return (bgame_str_t){ 0 };
	}
}

static inline void
bgame_init_script(bgame_script_instance_t* script, buxn_vm_t* vm) {
	buxn_vm_reset(vm, BUXN_VM_RESET_STACK);
	bgame_unpack_script_instance(script, vm);
	buxn_vm_execute(vm, BUXN_RESET_VECTOR);
	bgame_pack_script_instance(script, vm);
}

static inline void
bgame_run_script(
	bgame_script_instance_t* script,
	uint8_t vector_port,
	buxn_vm_t* vm
) {
	uint16_t vector_addr = buxn_vm_load2(script->device, vector_port, BUXN_DEV_ADDR_MASK);
	if (vector_addr != 0) {
		buxn_vm_reset(vm, BUXN_VM_RESET_STACK);
		bgame_unpack_script_instance(script, vm);
		buxn_vm_execute(vm, vector_addr);
		bgame_pack_script_instance(script, vm);
	}
}

#endif
