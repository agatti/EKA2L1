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

#include <epoc/services/window/classes/scrdvc.h>
#include <epoc/services/window/classes/wingroup.h>
#include <epoc/services/window/classes/winuser.h>
#include <epoc/services/window/op.h>
#include <epoc/services/window/opheader.h>
#include <epoc/services/window/window.h>
#include <epoc/utils/sec.h>

#include <common/cvt.h>
#include <common/e32inc.h>
#include <common/log.h>

#include <e32err.h>

namespace eka2l1::epoc {
    static epoc::security_policy key_capture_policy({ epoc::cap_sw_event });

    void window_group::lost_focus() {
        queue_event(epoc::event{ id, epoc::event_code::focus_lost });
    }

    void window_group::gain_focus() {
        queue_event(epoc::event{ id, epoc::event_code::focus_gained });
    }

    void window_group::receive_focus(service::ipc_context &context, ws_cmd &cmd) {
        flags &= ~focus_receiveable;

        if (*reinterpret_cast<bool *>(cmd.data_ptr)) {
            flags |= focus_receiveable;

            LOG_TRACE("Request group {} to enable keyboard focus", common::ucs2_to_utf8(name));
        } else {
            LOG_TRACE("Request group {} to disable keyboard focus", common::ucs2_to_utf8(name));
        }

        scr->update_focus(&client->get_ws(), nullptr);
        context.set_request_status(KErrNone);
    }

    window_group::window_group(window_server_client_ptr client,  screen *scr, epoc::window *parent)
        : window(client, scr, parent, window_kind::group)
        , flags(0) {
        // Create window group as child
        top = std::make_unique<window_top_user>(client, scr, this);
        child = top.get();
    }

    
    void window_group::set_text_cursor(service::ipc_context &context, ws_cmd &cmd) {
        // Warn myself in the future!
        LOG_WARN("Set cursor text is mostly a stubbed now");

        ws_cmd_set_text_cursor *cmd_set = reinterpret_cast<decltype(cmd_set)>(cmd.data_ptr);
        auto window_user_to_set = reinterpret_cast<window_user*>(client->get_object(cmd_set->win));

        if (!window_user_to_set || window_user_to_set->type == window_kind::client) {
            LOG_ERROR("Window not found or not client kind to set text cursor");
            context.set_request_status(KErrNotFound);
            return;
        }

        window_user_to_set->cursor_pos = cmd_set->pos + window_user_to_set->pos;
        context.set_request_status(KErrNone);   
    }

    void window_group::execute_command(service::ipc_context &ctx, ws_cmd &cmd) {
        bool result = execute_command_for_general_node(ctx, cmd);

        if (result) {
            return;
        }

        TWsWindowOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsWinOpEnableScreenChangeEvents: {
            epoc::event_screen_change_user evt;
            evt.user = this;

            client->add_event_notifier<epoc::event_screen_change_user>(evt);
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsWinOpCaptureKey: case EWsWinOpCaptureKeyUpsAndDowns: {
            if (!ctx.satisfy(key_capture_policy)) {
                ctx.set_request_status(KErrPermissionDenied);
                break;
            }

            ws_cmd_capture_key *capture_key_cmd = reinterpret_cast<decltype(capture_key_cmd)>(cmd.data_ptr);

            epoc::event_capture_key_notifier capture_key_notify;
            capture_key_notify.keycode_ = capture_key_cmd->key;
            capture_key_notify.modifiers_ = capture_key_cmd->modifiers;
            capture_key_notify.modifiers_mask_ = capture_key_cmd->modifier_mask;
            capture_key_notify.type_ = (op == EWsWinOpCaptureKeyUpsAndDowns) ? epoc::event_key_capture_type::up_and_downs : 
                epoc::event_key_capture_type::normal;
            capture_key_notify.pri_ = capture_key_cmd->priority;

            ctx.set_request_status(client->add_event_notifier(capture_key_notify));

            break;
        }

        case EWsWinOpSetName: {
            auto name_re = ctx.get_arg<std::u16string>(remote_slot);

            if (!name_re) {
                ctx.set_request_status(KErrArgument);
                break;
            }

            name = std::move(*name_re);
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsWinOpEnableOnEvents: {
            LOG_TRACE("Currently not support lock/unlock event for window server");
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsWinOpReceiveFocus: {
            receive_focus(ctx, cmd);
            break;
        }

        case EWsWinOpSetTextCursor: {
            set_text_cursor(ctx, cmd);
            break;
        }

        case EWsWinOpOrdinalPosition: {
            ctx.set_request_status(ordinal_position(true));
            break;
        }

        case EWsWinOpOrdinalPriority: {
            ctx.set_request_status(priority);
            break;
        }

        default: {
            LOG_ERROR("Unimplemented window group opcode 0x{:X}!", cmd.header.op);
            break;
        }
        }
    }
}
