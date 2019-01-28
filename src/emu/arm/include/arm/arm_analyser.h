/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <common/types.h>

#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>

namespace eka2l1::arm {
    enum reg {
        R0,
        R1,
        R2,
        R3,
        R4,
        R5,
        R6,
        R7,
        R8,
        R9,
        R10,
        R11,
        R12,
        R13,
        R14,
        R15,
        // TODO: VFP NEON registers
        SP = R13,
        LR = R14,
        PC = R15
    };

    enum class instruction {
    #define INST(x) x,
        #include <arm/arm_opcodes.def>
    #undef INST
    };

    enum class cc {
        INVALID,
        EQ,
        NE,
        HS,
        LO,
        MI,
        PL,
        VS,
        VC,
        HI,
        LS,
        GE,
        LT,
        GT,
        LE,
        AL
    };

    enum class arm_disassembler_backend {
        capstone = 0,       ///< Using capstone to analyse and disassemble instruction.
        homemade = 2        ///< Using homemade decoder. Not really smart
    };

    enum class arm_instruction_type {
        arm,
        thumb16,
        thumb32
    };

    enum arm_instruction_group {
        group_branch = 0x100,
        group_interrupt = 0x200,
        group_branch_relative = 0x400
    };

    struct arm_function {
        vaddress addr;
        std::size_t size;

        // Offset in this function - size of the block
        std::unordered_map<vaddress, std::size_t> blocks;
    };

    struct arm_instruction_base {
        union {
            // For thumb32 and arm
            std::uint32_t opcode;
            std::uint16_t opcode16;
        };

        arm_instruction_type inst_type;
        instruction iname;

        std::uint32_t group;
        std::uint8_t  size;

        cc cond;

        virtual ~arm_instruction_base() {}
        
        /*! \brief Get the disassembled result in text format.
        */
        virtual std::string mnemonic() = 0;

        /*! \brief Get all the registers which are being read by this instruction.
         *
         * \returns A vector contains all registers suited.
        */
        virtual std::vector<arm::reg> get_regs_read() = 0;
        
        /*! \brief Get all the registers which are being written by this instruction.
         *
         * \returns A vector contains all registers suited.
        */
        virtual std::vector<arm::reg> get_regs_write() = 0;
    };

    class arm_analyser {
        std::uint8_t *code;
        std::size_t   size;

        /* False = Thumb, True = ARM
        */
        bool          mode;
    public:
        explicit arm_analyser()
            : code(nullptr), size(0), mode(false) {
        }

        virtual std::uint32_t read(vaddress addr) = 0;

        /* Do analyse, starting from an address
        */
        void analyse(vaddress addr, std::vector<arm_function> funcs);

        /*! \brief Get the next instruction disassembled.
         *
         * If the analyser reaches the limit, it will return a nullptr.
        */
        virtual std::shared_ptr<arm_instruction_base> next_instruction(vaddress addr) {
            return nullptr;
        }
    };
}