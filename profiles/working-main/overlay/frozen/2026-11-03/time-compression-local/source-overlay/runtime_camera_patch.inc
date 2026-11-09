/* DriveClub1.28 native-Q camera response, selected by existing BB_CAMERA_PATCH.
   Diagnostic component flags and trace files remain explicit opt-ins. */
#ifdef _WIN32
#include <math.h>
#include "camera-fix-bootstrap.inc"
#include "head-staged-publication-api.inc"
static int camera_pose_read(uintptr_t address,void *out,size_t size) {
    SIZE_T got=0;
    return address>=0x10000 && ReadProcessMemory(GetCurrentProcess(),(void *)address,out,size,&got) && got==size;
}
static void *camera_rate_hook(size_t,const unsigned char *,size_t,uintptr_t);
/* Private source-only callbacks; no guest getters, native hooks or raw writes. */
static void lsq_runtime_init(void);
static void lsq_runtime_forget(uintptr_t,uintptr_t);
static void lsq_complete_before_restore(void*,const void*);
static void lsq_parent_status(unsigned,void*,uintptr_t,float,int64_t,int64_t,int64_t);
static void lsqt_init(void);
static void lsqt_invalidate_locked(uintptr_t,uintptr_t);
static int lsqt_commit_binding(const void*);
static void lsqt_role_event(unsigned,uintptr_t);
static void lsqm_init(void);
static void lsqm_observe(const void*,const void*,int64_t,int,int);
#include "runtime_camera_accuracy_audit.inc"
#include "runtime_camera_rate.inc"
#include "runtime_camera_motion.inc"
#include "runtime_replay_recorded_motion.inc"
#include "runtime_replay_gforce.inc"
#include "runtime_replay_driver_observation.inc"
static int replay_linear_q_requested(void);
static int replay_linear_q_install(void);
static void replay_linear_q_discontinuity(void);
static void replay_linear_q_body_reset(uintptr_t);
static int lfpo_enabled;
static ABI void lfpo_hsq_before(void*,float);
static ABI void lfpo_hsq_after(void*,float);
#include "runtime_replay_hud_observation.inc"
#include "replay-packet-observer.inc"
#include "feedback-target-basis-observer.inc"
#include "feedback-branch-scalar-observer.inc"
#include "runtime_replay_feedback.inc"
#include "runtime_lf_transaction_tape.inc"
#include "angular-live-runtime.inc"
static void gfr_forget(uintptr_t);
#include "native-feedback32-observer.inc"
static int replay_linear_mean_rebase_requested(void);
static int replay_linear_mean_rebase_prepare(void);
static void replay_linear_mean_rebase_commit(void);
static int feedback_branch_scalar_install(void);
#include "replay-linear-native-q.inc"
#include "late-sensor-q-runtime.inc"
#include "late-q-mode-observer.inc"
#include "replay-linear-mean-rebase.inc"
static void rpq_actor_invalidate(uintptr_t);
static void rpq_lifecycle_observer_ready(unsigned);
static int rpl_enabled;
#include "reference-seek.inc"
#include "hud-display-trace.inc"
#include "gforce-output-observer.inc"
#include "sdk-rebase-observer.inc"
#include "low-fps-physics-observer.inc"
#include "runtime-lifecycle.inc"
#include "paired-query-runtime.inc"
#include "late-sensor-q-tombstone-runtime.inc"
#include "runtime-head-selector.inc"
#include "runtime-head-pause.inc"
#include "head-response-primitives-runtime.inc"
#include "head-staged-publication-runtime.inc"
#include "head-temporal-group-runtime.inc"
#include "head-temporal-group-publication-binding.inc"
static void camera_patch_install(void) {
    camera_fix_bootstrap_configure();
    hgg_runtime_prepare();
    hps_install();
    hpr_group_bind_epoch_provider(htp_source_epoch);
    hpr_install();
    hds_install();
    rpl_install();
    lsq_runtime_init();
    lsqm_init();
    lsqt_init();
    if(!replay_linear_q_requested())feedback_branch_scalar_install(); /* RLQ installs42 after its whole-region hash */
    feedback_target_basis_enabled=feedback_target_basis_requested();
    rpq_runtime_install(); /* clone ORIGINAL private controller before any detour */
    replay_packet_observer_install();paired_query_install();
    gfo_install();
    sdk_rebase_observer_install();
    lf_transaction_tape_install();
    const char *enabled=getenv("BB_CAMERA_PATCH");
    const char *oracle=camera_fix_getenv("BB_CAMERA_ACCURACY_ORACLE");camera_accuracy_oracle=oracle&&!strcmp(oracle,"1");
    if((!enabled || strcmp(enabled,"1"))&&!camera_accuracy_oracle)return;
    static const unsigned char update[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xe4,0xc0,0xff,0xff,0xff};
    static const unsigned char impulse[]={0x55,0x48,0x89,0xe5,0x53,0x50,0xc5,0xf8,0x28,0xc8,0x48,0x89,0xfb};
    static const unsigned char noise[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x18};
    if(runtime_image_size<0xcc8820+sizeof update ||
       memcmp(image+0xcc8820,update,sizeof update) || memcmp(image+0x6ce6f0,impulse,sizeof impulse) ||
       runtime_image_size<0xce3540+sizeof noise || memcmp(image+0xce3540,noise,sizeof noise)) {
        puts("CameraPatch: unsupported 1.28 signatures; skipped");return;
    }
    camera_rate_update_original=(CameraRateUpdate)camera_rate_hook(0xcc8820,update,sizeof update,(uintptr_t)camera_rate_update);
    if(!camera_rate_update_original)return;
    hud_display_trace_install();
    hkt_install();
    camera_reference_seek_install();
    hgg_htp_bind();
    htp_install();
    hcal_install();
    hat_install();
    physics_source_tape_install();
    if(camera_accuracy_oracle){
        replay_recorded_motion_enabled=0;
        if(camera_fix_getenv("BB_CAMERA_REFERENCE_REPLAY_FEEDBACK"))replay_feedback_install();
        camera_dynamics_install();camera_motion_install();replay_hud_install();low_fps_physics_observer_install();
        if(camera_accuracy_trace_active()||getenv("BB_CAMERA_REPLAY_CONTROL"))camera_accuracy_provider_install();
        if(camera_noise_lifecycle_install())camera_noise_update_original=(CameraNoiseUpdate)camera_rate_hook(0xce3540,noise,sizeof noise,(uintptr_t)camera_noise_update);
        puts("CameraResponse: proven rigid HF reference composed with isolated response candidate");
        puts("CameraResponse: LIVE LF/HF/chase/HUD reference candidate; remaining driver/angular math native");return;
    }
    replay_recorded_motion_enabled=1;
    camera_dynamics_install();
    camera_motion_install();
    replay_feedback_install();
    replay_gforce_install();
    printf("CameraPatch: driver observer %s\n",replay_driver_observation_patch()?"on":"unavailable");
    replay_hud_install();low_fps_physics_observer_install();
    camera_rate_impulse_original=(CameraRateImpulse)camera_rate_hook(0x6ce6f0,impulse,sizeof impulse,(uintptr_t)camera_rate_impulse);
    if(camera_noise_lifecycle_install())camera_noise_update_original=(CameraNoiseUpdate)camera_rate_hook(0xce3540,noise,sizeof noise,(uintptr_t)camera_noise_update);
    puts("CameraPatch: experimental high-FPS camera/replay workaround enabled; behavior is not reference-accurate");
}
#else
static void camera_patch_install(void) {}
#endif
