# Interrupts

OSFIX interrupt support is split between kernel CPU setup and the APIC module.

## GDT and IDT

The kernel initializes the GDT and IDT before enabling ISR handling.

Key files:

- `kernel/main/cpu/gdt.c`
- `kernel/main/cpu/idt.c`
- `kernel/includes/main/cpu/gdt.h`
- `kernel/includes/main/cpu/idt.h`

The IDT API allows gates to be set, enabled, and disabled.

## ISR Layer

ISR support lives in:

- `kernel/main/cpu/isr.c`
- `kernel/main/cpu/isrs_gen.c`
- `kernel/includes/main/cpu/asm/isr_stubs.asm`

`ISR_Initialize` installs generated gates, registers interrupt 14 as the page
fault handler, and enables the rest of the IDT gates.

`ISR_Handler` dispatches through `g_ISRHandlers`. If no handler exists, it:

- Prints `KERNEL PANIC`.
- Logs the interrupt number.
- Prints verbose exception context.
- Dumps general-purpose, control, debug, GDT, and IDT registers.
- Halts.

The page fault handler prints page-fault-specific context, including CR2 and
the error code, then dumps registers and halts.

## Registering Handlers

Use:

```c
ISR_RegisterHandler(interrupt, handler);
```

This stores the handler and enables the matching IDT gate.

## APIC Module

APIC code lives under `drivers/pic/apic`.

The APIC module entry point:

1. Disables the legacy PIC by remapping and masking both PICs.
2. Enables APIC support in CR4.
3. Sets the IA32 APIC base MSR enable bit if needed.
4. Enables the local APIC through the spurious interrupt vector register.

The module is loaded early from `/apic/apic.elf`.

## APIC IRQ Layer

`apic_irq.c` provides an IRQ dispatch table for up to 64 IRQs. IRQ vectors are
treated as `0x20 + irq`.

Important functions:

- `APIC_IRQ_Initialize`: initializes APIC and registers the APIC IRQ handler
  across the IRQ vector range.
- `APIC_IRQ_RegisterHandler`: stores a handler for an IRQ number.
- `APIC_EnableIRQ` and `APIC_DisableIRQ`: mask or unmask local APIC LVT entries.
- `APIC_EnableIOIRQ` and `APIC_DisableIOIRQ`: update IOAPIC redirection entries.
- `APIC_IRQ_Handler`: dispatches to the registered handler and sends EOI.

## Current Caveats

- APIC initialization is module-based, so anything requiring APIC must run after
  the APIC module loads.
- IRQ dependency ordering is manual.
- Unhandled APIC IRQs are logged and acknowledged with EOI.
