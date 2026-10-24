// File-based IPC for the WPF shell. Settings/patch selection and launch remain native.
using Json = nlohmann::json;
void WriteJsonFile(const std::wstring& path, const Json& value) {
  const auto temp=path+L".tmp";
  {std::ofstream out(temp.c_str(),std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("Cannot write UI response");out<<value.dump(2);}
  if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot publish UI response");
}
void WriteExternalProgress(int percent,const std::wstring& text) {
  if(g_external_progress.empty())return;
  try{WriteJsonFile(g_external_progress,{{"Percent",percent},{"Text",Narrow(text)}});}catch(...){}
}
Json UiState(const std::wstring& error=L"") {
  Json controls=Json::array(),cards=Json::array();
  for(size_t i=0;i<g_c.size();++i){
    const auto& c=g_c[i];Json options=Json::array();int selected=c.sel;
    std::string kind=c.kind==Kind::Toggle?"Toggle":c.kind==Kind::Action?"Action":c.kind==Kind::Slider?"Slider":c.kind==Kind::Button?"Button":c.patches[0]=="@name"?"Text":"Choice";
    if(int(i)==g_lang){for(auto* n:kLangName)options.push_back(Narrow(n));selected=g_lang_id;}
    else if(int(i)==g_btrack){for(const auto& x:kBenchTracks)options.push_back(Narrow(std::wstring(x.name)+L" ("+x.group+L")"));selected=g_bench_track;}
    else if(int(i)==g_bcar){for(const auto& x:kBenchCars)options.push_back(Narrow(x.name));selected=g_bench_car;}
    else for(const auto& x:c.items)options.push_back(Narrow(x));
    controls.push_back({{"Id",i},{"Label",Narrow(c.label)},{"Hint",Narrow(c.hint)},{"Kind",kind},{"Tag",c.patches[0]},
      {"Minimum",c.min},{"Maximum",c.max},{"Unit",Narrow(c.unit)},{"Hidden",RowHidden(c)},{"Enabled",!BlurRowLocked(c)},
      {"Options",options},{"Selected",selected},{"Text",c.patches[0]=="@name"?Narrow(g_name):""}});
  }
  // Preserve upstream category, card, control order and Driving subpages directly.
  for(const auto& c:g_cards) {
    cards.push_back({{"Title",Narrow(c.title)},{"Page",c.tab},{"Subpage",c.sub},
      {"Controls",c.ctls},{"Note",c.note?Narrow(c.note):""}});
  }

  return {{"Version",2},{"Controls",controls},{"Cards",cards},{"UpdatePath",Narrow(g_folder[0].path)},{"BasePath",Narrow(g_folder[1].path)},
    {"FoldersOk",FoldersOk()},{"FolderProblem",Narrow(!FolderProblem(0).empty()?FolderProblem(0):FolderProblem(1))},
    {"HardwareProblem",Narrow(g_gpu_problem)},{"RuntimeAvailable",Exists(GameExe())},{"Error",Narrow(error)},
    {"QuickControls",{g_outres,g_aa,g_preset,g_fps}},{"PresetId",g_preset},
    {"BenchmarkFps",Narrow(g_bench_result.fps)},{"BenchmarkLows",Narrow(g_bench_result.lows)},{"BenchmarkAbout",Narrow(g_bench_result.about)}};
}
void ImportUiState(const Json& input) {
  g_folder[0].path=Widen(input.value("UpdatePath",Narrow(g_folder[0].path)));
  g_folder[1].path=Widen(input.value("BasePath",Narrow(g_folder[1].path)));
  for(const auto& item:input.at("Controls")){
    const int id=item.at("Id").get<int>();if(id<0||id>=int(g_c.size()))throw std::runtime_error("Invalid control ID");
    auto& c=g_c[id];const int selected=item.value("Selected",c.sel);
    if(c.kind==Kind::Action || c.kind==Kind::Button)continue;
    if(c.patches[0]=="@name"){
      const std::string name=item.value("Text",Narrow(g_name));
      if(name.empty()||name.size()>20||std::any_of(name.begin(),name.end(),[](unsigned char ch){return ch<32||ch>126;}))throw std::runtime_error("Player name must contain 1-20 printable Latin characters");
      g_name=Widen(name);c.items[0]=g_name;
    }else if(id==g_lang){if(selected<0||selected>=21)throw std::runtime_error("Invalid language");g_lang_id=selected;c.items[0]=kLangName[selected];}
    else if(id==g_btrack){if(selected<0||selected>=kBenchTrackCount)throw std::runtime_error("Invalid track");g_bench_track=selected;}
    else if(id==g_bcar){if(selected<0||selected>=kBenchCarCount)throw std::runtime_error("Invalid car");g_bench_car=selected;}
    else if(c.kind==Kind::Slider){if(selected<c.min||selected>c.max)throw std::runtime_error("Invalid slider value");c.sel=selected;}
    else {const int count=c.kind==Kind::Toggle?2:int(c.items.size());if(selected<((id==g_drive_preset||id==g_ffb_preset)?-1:0)||selected>=count)throw std::runtime_error("Invalid setting value");c.sel=selected;}
    if(c.patches[0]=="@skipgpu")g_skip_gpu_check=c.sel!=0;
  }
  NameNativeUpscale();ShowBenchChoices();CheckFolders();
}
int RunWpfBridge(int argc,wchar_t** argv) {
  if(argc<3||wcsncmp(argv[1],L"--ui-",5)!=0)return -1;
  const std::wstring command=argv[1];
  const std::wstring output=argv[argc-1];
  try{
    if(command==L"--ui-state"){
      if(!g_skip_gpu_check){g_gpu_problem=CheckCpu();if(g_gpu_problem.empty())g_gpu_problem=CheckGpu();}
    }else if(argc>=4){
      std::ifstream in(argv[2],std::ios::binary);const Json request=Json::parse(in);ImportUiState(request);
      if(command==L"--ui-reset-page"){
        const int page=request.value("ResetPage",-2), sub=request.value("ResetSubpage",-1);
        if(page < -1 || page >= kTabCount || (page==kTabDriving && (sub<0 || sub>=kSubCount)))
          throw std::runtime_error("Invalid reset page");
        std::vector<int> ids;
        if(page==-1)ids={g_outres,g_aa,g_preset,g_fps};
        else for(const auto& card:g_cards)if(card.tab==page && (page!=kTabDriving || card.sub==sub))
          ids.insert(ids.end(),card.ctls.begin(),card.ctls.end());
        for(int id:ids){
          auto& c=g_c[id];
          if(c.kind==Kind::Action || c.kind==Kind::Button || c.patches[0]=="@name" || id==g_lang)continue;
          c.sel=c.def;
          if(id==g_btrack)g_bench_track=0;
          if(id==g_bcar)g_bench_car=kBenchDefaultCar;
          if(c.patches[0]=="@skipgpu")g_skip_gpu_check=c.sel!=0;
        }
        if(page==-1 || page==kTabGraphics)DerivePreset();
        if(page==kTabDriving && sub==kSubAssists)DeriveDrivePreset();
        if(page==kTabDriving && sub==kSubWheel)DeriveWheelPreset();
        NameNativeUpscale();ShowBenchChoices();
      }
      else if(command==L"--ui-validate"){
        const int changed=request.value("ChangedId",-1);
        if(changed==g_wide && g_c[g_wide].sel>0)g_c[g_outres].sel=g_c[g_inres].sel=0;
        DerivePreset();DeriveDrivePreset();DeriveWheelPreset();
      }
      else if(command==L"--ui-preset")ApplyPreset(g_c[g_preset].sel);
      else if(command==L"--ui-drive-preset"){if(g_c[g_drive_preset].sel>=0)ApplyDrivePreset(g_c[g_drive_preset].sel);}
      else if(command==L"--ui-wheel-preset"){if(g_c[g_ffb_preset].sel>=0)ApplyWheelPreset(g_c[g_ffb_preset].sel);}
      else if(command==L"--ui-wheelsetup"){OpenWheelSetup();}
      else if(command==L"--ui-apply"||command==L"--ui-run"||command==L"--ui-benchmark"){
        DerivePreset();DeriveDrivePreset();DeriveWheelPreset();SaveSettings();
        if(command!=L"--ui-apply"){
          if(!FoldersOk()){WriteJsonFile(output,UiState(L"Choose valid game folders on Play."));return 1;}
          if(!g_skip_gpu_check){g_gpu_problem=CheckCpu();if(g_gpu_problem.empty())g_gpu_problem=CheckGpu();}
          if(GpuBlocked()){WriteJsonFile(output,UiState(g_gpu_problem));return 1;}
          g_external_progress=output+L".progress";StartLauncherLog();
          if(command==L"--ui-benchmark")BenchWorker();
          else {
            SetProgress(0,L"Preparing the game...");std::wstring error=Preflight();
            if(error.empty())error=RunPrepare();
            if(error.empty()){SetProgress(100,L"Starting DRIVECLUB...");error=StartGame();}
            WriteJsonFile(output,UiState(error));return error.empty()?0:1;
          }
          LoadBenchResult();WriteJsonFile(output,UiState(g_status_color.GetValue()==kWarn.GetValue()?g_status:L""));return 0;
        }
      }else throw std::runtime_error("Unknown UI command");
    }else throw std::runtime_error("Missing UI request");
    WriteJsonFile(output,UiState());return 0;
  }catch(const std::exception& ex){
    try{WriteJsonFile(output,{{"Error",ex.what()}});}catch(...){}
    return 1;
  }
}
