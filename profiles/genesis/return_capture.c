/* Bounded all-path proof for return-address consumers. */
#include "return_capture.h"

#include "m68k_decoder.h"

#define RETURN_CAPTURE_MAX_NODES 96

typedef enum {
    PROOF_VISITING,
    PROOF_TRUE,
    PROOF_FALSE,
} ProofState;

typedef struct {
    uint32_t pc;
    unsigned popped;
    ProofState state;
} ProofNode;

typedef struct {
    const GenesisRom *rom;
    ProofNode nodes[RETURN_CAPTURE_MAX_NODES];
    int node_count;
    bool allow_plain_exit;
    bool has_capturing_exit;
} ProofContext;

static bool ea_uses_sp(int ea)
{
    return ea >= (EA_An << 3) && ea <= ((EA_An_IDX << 3) | 7)
        && (ea & 7) == 7;
}

static unsigned return_pop_bytes(const M68KInstr *ins)
{
    if ((ins->size != M68K_SIZE_W && ins->size != M68K_SIZE_L)
            || ins->src_ea != ((EA_An_POST << 3) | 7)
            || ea_uses_sp(ins->dst_ea))
        return 0;
    unsigned width = ins->size == M68K_SIZE_L ? 4u : 2u;
    if (ins->mnemonic == MN_MOVE || ins->mnemonic == MN_MOVEA)
        return width;
    /* Loading A7 rebuilds the frame rather than merely consuming slots. */
    if (ins->mnemonic == MN_MOVEM && ((ins->words[0] >> 10) & 1) != 0
            && ins->words[1] != 0 && (ins->words[1] & 0x8000) == 0) {
        unsigned count = 0;
        for (unsigned bit = 0; bit < 16; bit++)
            count += (ins->words[1] >> bit) & 1u;
        return count * width;
    }
    return 0;
}

static bool invalid_stack_operation(const M68KInstr *ins)
{
    /* Apart from the first capture and final RTS, do not prove through SP
     * operands, frame changes or calls. This deliberately includes harmless
     * reads: a false result selects ordinary call handling, not missing code.
     * Some decoder operands are implicit, so check those separately. */
    if (ea_uses_sp(ins->src_ea) || ea_uses_sp(ins->dst_ea)
            || m68k_is_call(ins))
        return true;

    switch (ins->mnemonic) {
    case MN_PEA:
    case MN_LINK:
    case MN_UNLK:
    case MN_MOVEM:
    case MN_MOVE_USP:
    case MN_MOVEC:
        return true;
    case MN_LEA:
    case MN_ADDA:
    case MN_SUBA:
        return ins->reg == 7;
    case MN_EXG: {
        uint16_t opcode = ins->words[0];
        return ((opcode & 0xF1F8) == 0xC148
                    && (((opcode >> 9) & 7) == 7 || (opcode & 7) == 7))
            || ((opcode & 0xF1F8) == 0xC188 && (opcode & 7) == 7);
    }
    case MN_ADDX:
    case MN_SUBX:
    case MN_ABCD:
    case MN_SBCD:
        return ins->predec_mem_form
            && (ins->reg == 7 || (ins->src_ea & 7) == 7);
    case MN_CMPM:
        return ins->reg == 7 || (ins->src_ea & 7) == 7;
    default:
        return false;
    }
}

static bool prove_from(ProofContext *ctx, uint32_t pc, unsigned popped)
{
    for (int i = 0; i < ctx->node_count; i++) {
        if (ctx->nodes[i].pc == pc && ctx->nodes[i].popped == popped)
            return ctx->nodes[i].state == PROOF_TRUE;
    }

    /* Cycles (VISITING), unknown exits and budget exhaustion are unproven. */
    if (ctx->node_count >= RETURN_CAPTURE_MAX_NODES || (pc & 1) != 0
            || pc >= ctx->rom->rom_size || ctx->rom->rom_size - pc < 2)
        return false;

    int node_index = ctx->node_count++;
    ctx->nodes[node_index].pc = pc;
    ctx->nodes[node_index].popped = popped;
    ctx->nodes[node_index].state = PROOF_VISITING;

    M68KInstr ins;
    bool result = false;
    if (!m68k_decode(ctx->rom, pc, &ins) || ins.byte_length < 2
            || ins.byte_length > ctx->rom->rom_size - pc)
        goto done;

    uint32_t fallthrough = pc + ins.byte_length;
    unsigned pop_bytes = return_pop_bytes(&ins);
    if (pop_bytes && (ctx->allow_plain_exit
            || (!popped && pop_bytes == 4 && ins.size == M68K_SIZE_L))) {
        result = prove_from(ctx, fallthrough, popped + pop_bytes);
        goto done;
    }
    if (invalid_stack_operation(&ins))
        goto done;

    switch (ins.mnemonic) {
    case MN_RTS: {
        /* Local unwind may consume several complete slots, including slots
         * popped as words. Only a single longword permits an unconditional
         * call-site tail transfer; partial slots remain unproven. */
        bool whole_slots = popped >= 4 && (popped & 3u) == 0;
        ctx->has_capturing_exit |= whole_slots;
        result = ctx->allow_plain_exit ? (popped & 3u) == 0 : popped == 4;
        break;
    }
    case MN_Bcc:
    case MN_DBcc:
        if (ins.has_target)
            result = prove_from(ctx, ins.target_addr, popped)
                  && prove_from(ctx, fallthrough, popped);
        break;
    case MN_BRA:
    case MN_JMP:
        if (ins.has_target)
            result = prove_from(ctx, ins.target_addr, popped);
        break;
    case MN_OTHER:
    case MN_STOP:
    case MN_TRAP:
    case MN_TRAPV:
    case MN_CHK:
        break;
    default:
        if (!m68k_is_terminator(&ins))
            result = prove_from(ctx, fallthrough, popped);
        break;
    }

done:
    ctx->nodes[node_index].state = result ? PROOF_TRUE : PROOF_FALSE;
    return result;
}

bool m68k_return_capture_is_unconditional(const GenesisRom *rom, uint32_t start)
{
    if (!rom)
        return false;
    ProofContext ctx = {0};
    ctx.rom = rom;
    return prove_from(&ctx, start, 0);
}

bool m68k_return_capture_has_capturing_exit(const GenesisRom *rom, uint32_t start)
{
    if (!rom)
        return false;
    ProofContext ctx = {0};
    ctx.rom = rom;
    ctx.allow_plain_exit = true;
    return prove_from(&ctx, start, 0) && ctx.has_capturing_exit;
}
