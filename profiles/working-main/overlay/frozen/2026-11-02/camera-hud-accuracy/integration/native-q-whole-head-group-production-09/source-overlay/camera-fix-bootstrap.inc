/* Internal STARTUP policy for the original single BB_CAMERA_PATCH switch.
 * No environment/config/save mutation and no diagnostic path defaults.
 * Explicit legacy HEAD_REFERENCE=1 suppresses implicit whole-bank selection.
 * Contradictory head selectors fail closed for BOTH head response writers. */
#ifndef CAMERA_FIX_BOOTSTRAP_INCLUDED
#define CAMERA_FIX_BOOTSTRAP_INCLUDED
static int camera_fix_bootstrap_defaults,camera_fix_head_conflict;
static const char*camera_fix_head_reference_default;
static const char*camera_fix_head_group_default;
static const char*camera_fix_head_write_default;
static int camera_fix_binary_value(const char*v){return !v||!strcmp(v,"0")||!strcmp(v,"1");}
static int camera_fix_one(const char*v){return v&&!strcmp(v,"1");}
static int camera_fix_zero(const char*v){return v&&!strcmp(v,"0");}
static void camera_fix_bootstrap_configure(void){
 const char*patch=getenv("BB_CAMERA_PATCH"),*oracle=getenv("BB_CAMERA_ACCURACY_ORACLE");
 const char*head=getenv("BB_CAMERA_HEAD_REFERENCE"),*group=getenv("BB_CAMERA_HEAD_GROUP"),*write=getenv("BB_CAMERA_TEMPORAL_GROUP_WRITE");
 camera_fix_bootstrap_defaults=patch&&!strcmp(patch,"1")&&(!oracle||!strcmp(oracle,"1"));
 camera_fix_head_reference_default=camera_fix_head_group_default=camera_fix_head_write_default=NULL;
 camera_fix_head_conflict=!camera_fix_binary_value(head)||!camera_fix_binary_value(group)||!camera_fix_binary_value(write)||
  (camera_fix_one(head)&&(camera_fix_one(group)||camera_fix_one(write)))||
  (camera_fix_zero(group)&&camera_fix_one(write))||
  (oracle&&strcmp(oracle,"1")&&(camera_fix_one(group)||camera_fix_one(write)));
 if(camera_fix_head_conflict){puts("CameraResponseConfig: conflicting head selectors; legacy and whole-bank head writers disabled");return;}
 if(!camera_fix_bootstrap_defaults)return;
 if(camera_fix_one(head)){
  camera_fix_head_reference_default="1";camera_fix_head_group_default="0";camera_fix_head_write_default="0";
 }else{
  camera_fix_head_reference_default="0";
  camera_fix_head_group_default=camera_fix_zero(write)?"0":"1";
  camera_fix_head_write_default=camera_fix_zero(group)?"0":"1";
 }
}
static const char*camera_fix_getenv(const char*name){
 const char*explicit_value=getenv(name);
 int head=!strcmp(name,"BB_CAMERA_HEAD_REFERENCE"),group=!strcmp(name,"BB_CAMERA_HEAD_GROUP"),write=!strcmp(name,"BB_CAMERA_TEMPORAL_GROUP_WRITE");
 if((head||group||write)&&camera_fix_head_conflict)return "0";
 if(explicit_value||!camera_fix_bootstrap_defaults)return explicit_value;
 if(head)return camera_fix_head_reference_default;
 if(group)return camera_fix_head_group_default;
 if(write)return camera_fix_head_write_default;
 static const char*const flags[]={
  "BB_CAMERA_ACCURACY_ORACLE","BB_CAMERA_ANGULAR_INIT_SHARED_Q","BB_CAMERA_ANGULAR_REFERENCE",
  "BB_CAMERA_HEAD_LATE_Q","BB_CAMERA_HEAD_REAL_CLOCK","BB_CAMERA_LF_REFERENCE",
  "BB_CAMERA_LF_SHARED_Q_INPUT","BB_CAMERA_REFERENCE_REPLAY_FEEDBACK",
  "BB_CAMERA_REFERENCE_SPRINGS","BB_GFORCE_REFERENCE","BB_HEAD_CLOCK_SELECTOR","BB_HEAD_PAUSED_CLOCK",
  "BB_HUD_SHARED_Q","BB_NATIVE_PACKET_CACHE","BB_PRIVATE_LATE_Q","BB_PRIVATE_LATE_Q_TOMBSTONE",
  "BB_REPLAY_LINEAR_MEAN_REBASE","BB_REPLAY_LINEAR_NATIVE_Q","BB_REPLAY_PACKET_LIFECYCLE",
  "BB_REPLAY_PACKET_LIFECYCLE_ROLES","BB_REPLAY_PAIRED_QUERY_Q"
 };
 for(unsigned i=0;i<sizeof flags/sizeof*flags;++i)if(!strcmp(name,flags[i]))return "1";
 return NULL;
}
#endif
