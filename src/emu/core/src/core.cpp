#include <core.h>
#include <process.h>

#include <common/log.h>
#include <disasm/disasm.h>
#include <loader/eka2img.h>

namespace eka2l1 {
    void system::init() {
        // Initialize all the system that doesn't depend on others first
        timing.init();
        mem.init();
        asmdis.init();

        cpu = arm::create_jitter(&timing, &mem, &asmdis, arm::jitter_arm_type::unicorn);

        kern.init(&timing, cpu.get());
    }

    void system::load(const std::string &name, uint64_t id, const std::string &path) {
        auto img = loader::parse_eka2img(path);

        if (!img) {
            LOG_CRITICAL("This is not what i expected! Fake E32Image!");
            return;
        }

        loader::eka2img &real_img = img.value();
        mngr = std::make_unique<hle::lib_manager>("db.yml");

        bool succ = loader::load_eka2img(real_img, &mem, *mngr);

        if (!succ) {
            LOG_CRITICAL("Unable to load EKA2Img!");
            return;
        }

        crr_process = std::make_shared<process>(&kern, &mem, id, name, 
            real_img.rt_code_addr + real_img.header.entry_point,
            real_img.header.heap_size_min, real_img.header.heap_size_max,
            real_img.header.stack_size);

        crr_process->run();
    }

    int system::loop() {
        auto prepare_reschedule = [&]() {
            cpu->prepare_rescheduling();
            reschedule_pending = true;
        };

        if (kern.crr_thread() == nullptr) {
            timing.idle();
            timing.advance();
            prepare_reschedule();
        } else {
            timing.advance();
            cpu->run();
        }

        kern.reschedule();
        reschedule_pending = false;

        return 1;
    }

    void system::shutdown() {
        timing.shutdown();
        kern.shutdown();
        mem.shutdown();
        asmdis.shutdown();
    }
}
