#include "vf2/model2a.h"

vf2_status vf2_model2a_read_impl(
    const vf2_model2a *machine,
    uint32_t address,
    void *destination,
    size_t size
);

vf2_status vf2_model2a_write_impl(
    vf2_model2a *machine,
    uint32_t address,
    const void *source,
    size_t size
);

static void observe_access(
    const vf2_model2a *machine,
    vf2_model2a_memory_access_kind kind,
    uint32_t address,
    const void *data,
    size_t size
)
{
    vf2_model2a_memory_access access;
    if (machine == NULL || machine->memory_observer == NULL) {
        return;
    }
    access.kind = kind;
    access.address = address;
    access.data = data;
    access.size = size;
    machine->memory_observer(&access, machine->memory_observer_context);
}

vf2_status vf2_model2a_set_memory_observer(
    vf2_model2a *machine,
    vf2_model2a_memory_observer observer,
    void *context
)
{
    if (machine == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    machine->memory_observer = observer;
    machine->memory_observer_context = context;
    return VF2_OK;
}

vf2_status vf2_model2a_read(
    const vf2_model2a *machine,
    uint32_t address,
    void *destination,
    size_t size
)
{
    vf2_status status = vf2_model2a_read_impl(machine, address, destination, size);
    if (status == VF2_OK) {
        observe_access(machine, VF2_MODEL2A_MEMORY_READ, address, destination, size);
    }
    return status;
}

vf2_status vf2_model2a_write(
    vf2_model2a *machine,
    uint32_t address,
    const void *source,
    size_t size
)
{
    vf2_status status = vf2_model2a_write_impl(machine, address, source, size);
    if (status == VF2_OK) {
        observe_access(machine, VF2_MODEL2A_MEMORY_WRITE, address, source, size);
    }
    return status;
}
