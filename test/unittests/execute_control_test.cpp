#include "execute.hpp"
#include "utils.hpp"
#include <gtest/gtest.h>

using namespace fizzy;

TEST(execute_control, unreachable)
{
    Module module;
    module.codesec.emplace_back(Code{0, {Instr::unreachable, Instr::end}, {}});

    const auto [trap, ret] = execute(module, 0, {});

    ASSERT_TRUE(trap);
}

TEST(execute_control, nop)
{
    Module module;
    module.codesec.emplace_back(Code{0, {Instr::nop, Instr::end}, {}});

    const auto [trap, ret] = execute(module, 0, {});

    ASSERT_FALSE(trap);
    EXPECT_EQ(ret.size(), 0);
}

TEST(execute_control, block_br)
{
    // (local i32 i32)
    // block
    //   i32.const 0xa
    //   local.set 1
    //   br 0
    //   i32.const 0xb
    //   local.set 1
    // end
    // local.get 1

    Module module;
    module.codesec.emplace_back(Code{2,
        {Instr::block, Instr::i32_const, Instr::local_set, Instr::br, Instr::i32_const,
            Instr::local_set, Instr::end, Instr::local_get, Instr::end},
        from_hex("00"       /* arity */
                 "07000000" /* target_pc: 7 */
                 "1d000000" /* target_imm: 29 */
                 "0a000000" /* i32.const 0xa */
                 "01000000" /* local.set 1 */
                 "00000000" /* br 0 */
                 "0b000000" /* i32.const 0xb */
                 "01000000" /* local.set 1 */
                 "01000000" /* local.get 1 */
            )});

    const auto [trap, ret] = execute(module, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xa);
}

TEST(execute_control, loop_void_empty)
{
    Module module;
    module.codesec.emplace_back(
        Code{0, {Instr::loop, Instr::local_get, Instr::end, Instr::end}, {0, 0, 0, 0, 0}});

    const auto [trap, ret] = execute(module, 0, {1});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 1);
}

TEST(execute_control, loop_void_br_if_16)
{
    // This will try to loop 16 times.
    //
    // loop
    //   local.get 0 # This is the input argument.
    //   i32.const 1
    //   i32.sub
    //   local.tee 0
    //   br_if 0
    //   local.get 0
    // end
    Module module;
    module.codesec.emplace_back(Code{0,
        {Instr::loop, Instr::local_get, Instr::i32_const, Instr::i32_sub, Instr::local_tee,
            Instr::br_if, Instr::local_get, Instr::end, Instr::end},
        {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    const auto [trap, ret] = execute(module, 0, {16});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0);
}
