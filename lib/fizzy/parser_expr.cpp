#include "parser.hpp"

namespace fizzy
{
namespace
{
template <typename T>
inline void push(bytes& b, T value)
{
    b.resize(b.size() + sizeof(value));
    __builtin_memcpy(&b[b.size() - sizeof(value)], &value, sizeof(value));
}

/// The immediates offset for block instructions.
using label_position = size_t;

/// Sentinel constant to loop labels. They are not interested for parser.
constexpr auto loop_label = label_position(-1);

}  // namespace

parser_result<Code> parse_expr(const uint8_t* pos)
{
    Code code;
    std::vector<label_position> label_positions;
    bool continue_parsing = true;
    while (continue_parsing)
    {
        const auto instr = static_cast<Instr>(*pos++);
        switch (instr)
        {
        default:
            throw parser_error{"invalid instruction " + std::to_string(*(pos - 1))};

        case Instr::unreachable:
        case Instr::nop:
        case Instr::drop:
        case Instr::select:
        case Instr::memory_size:
        case Instr::memory_grow:
        case Instr::i32_eq:
        case Instr::i32_eqz:
        case Instr::i32_ne:
        case Instr::i32_lt_s:
        case Instr::i32_lt_u:
        case Instr::i32_gt_s:
        case Instr::i32_gt_u:
        case Instr::i32_le_s:
        case Instr::i32_le_u:
        case Instr::i32_ge_s:
        case Instr::i32_ge_u:
        case Instr::i64_eq:
        case Instr::i64_eqz:
        case Instr::i64_ne:
        case Instr::i64_lt_s:
        case Instr::i64_lt_u:
        case Instr::i64_gt_s:
        case Instr::i64_gt_u:
        case Instr::i64_le_s:
        case Instr::i64_le_u:
        case Instr::i64_ge_s:
        case Instr::i64_ge_u:
        case Instr::i32_clz:
        case Instr::i32_ctz:
        case Instr::i32_popcnt:
        case Instr::i32_add:
        case Instr::i32_sub:
        case Instr::i32_mul:
        case Instr::i32_div_s:
        case Instr::i32_div_u:
        case Instr::i32_rem_s:
        case Instr::i32_rem_u:
        case Instr::i32_and:
        case Instr::i32_or:
        case Instr::i32_xor:
        case Instr::i32_shl:
        case Instr::i32_shr_s:
        case Instr::i32_shr_u:
        case Instr::i32_rotl:
        case Instr::i32_rotr:
        case Instr::i64_clz:
        case Instr::i64_ctz:
        case Instr::i64_popcnt:
        case Instr::i64_add:
        case Instr::i64_sub:
        case Instr::i64_mul:
        case Instr::i64_div_s:
        case Instr::i64_div_u:
        case Instr::i64_rem_s:
        case Instr::i64_rem_u:
        case Instr::i64_and:
        case Instr::i64_or:
        case Instr::i64_xor:
        case Instr::i64_shl:
        case Instr::i64_shr_s:
        case Instr::i64_shr_u:
        case Instr::i64_rotl:
        case Instr::i64_rotr:
        case Instr::i32_wrap_i64:
        case Instr::i64_extend_i32_s:
        case Instr::i64_extend_i32_u:
            break;

        case Instr::end:
        {
            if (!label_positions.empty())
            {
                const auto label_pos = label_positions.back();
                label_positions.pop_back();
                if (label_pos != loop_label)  // If end of block instruction.
                {
                    const auto target_pc = static_cast<uint32_t>(code.instructions.size() + 1);
                    const auto target_imm = static_cast<uint32_t>(code.immediates.size());

                    // Set the imm values for block instruction.
                    auto* block_imm = code.immediates.data() + label_pos;
                    __builtin_memcpy(block_imm, &target_pc, sizeof(target_pc));
                    block_imm += sizeof(target_pc);
                    __builtin_memcpy(block_imm, &target_imm, sizeof(target_imm));
                }
            }
            else
                continue_parsing = false;
            break;
        }

        case Instr::block:
        {
            const auto type = *pos;
            uint8_t arity;
            if (type == EmptyType)
            {
                arity = 0;
                ++pos;
            }
            else
            {
                // Will throw in case of incorrect type.
                std::tie(std::ignore, pos) = parser<ValType>{}(pos);
                arity = 1;
            }

            code.immediates.push_back(arity);

            label_positions.push_back(code.immediates.size());  // Imm offset after arity.

            // Placeholders for immediate values, filled at the matching end instruction.
            push(code.immediates, uint32_t{0});  // Diff to the end instruction.
            push(code.immediates, uint32_t{0});  // Diff for the immediates.
            break;
        }

        case Instr::loop:
        {
            const auto type = *pos++;
            if (type != EmptyType)
                throw parser_error{"loop can only has type arity 0"};
            label_positions.push_back(loop_label);  // Mark as not interested.
            break;
        }

        case Instr::local_get:
        case Instr::local_set:
        case Instr::local_tee:
        case Instr::global_get:
        case Instr::global_set:
        case Instr::br:
        case Instr::br_if:
        {
            uint32_t imm;
            std::tie(imm, pos) = leb128u_decode<uint32_t>(pos);
            push(code.immediates, imm);
            break;
        }

        case Instr::i32_const:
        {
            int32_t imm;
            std::tie(imm, pos) = leb128s_decode<int32_t>(pos);
            push(code.immediates, static_cast<uint32_t>(imm));
            break;
        }

        case Instr::i64_const:
        {
            int64_t imm;
            std::tie(imm, pos) = leb128s_decode<int64_t>(pos);
            push(code.immediates, static_cast<uint64_t>(imm));
            break;
        }

        case Instr::i32_load:
        case Instr::i64_load:
        case Instr::i32_store:
        case Instr::i64_store:
        {
            // alignment
            std::tie(std::ignore, pos) = leb128u_decode<uint32_t>(pos);

            // offset
            uint32_t imm;
            std::tie(imm, pos) = leb128u_decode<uint32_t>(pos);
            push(code.immediates, imm);
            break;
        }
        }
        code.instructions.emplace_back(instr);
    }
    if (!label_positions.empty())
        throw parser_error{"code is not balanced (missing end statements)"};
    return {code, pos};
}
}  // namespace fizzy
