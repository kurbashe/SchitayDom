modded class MissionServer
{
    override void OnInit()
    {
        super.OnInit();
        GMapService.Reset();
        GMapService.Get().Init();
        GMapServerRPC.Init();
    }

    override void OnMissionFinish()
    {
        GMapServerRPC.Shutdown();
        GMapService.Reset();
        super.OnMissionFinish();
    }
};
