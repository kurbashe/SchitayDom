modded class MissionBase
{
    override UIScriptedMenu CreateScriptedMenu(int id)
    {
        if (id == 784193)
            return new GMapMenu;
        return super.CreateScriptedMenu(id);
    }
};

modded class MissionGameplay
{
    override void OnInit()
    {
        super.OnInit();
        GMapClient.Reset();
        GMapClientRPC.Init();
    }

    override void OnMissionFinish()
    {
        GMapClientRPC.Shutdown();
        GMapClient.Reset();
        super.OnMissionFinish();
    }

    override void OnUpdate(float timeslice)
    {
        // Open before vanilla checks M: an existing menu prevents a second map.
        UAInput mapInput = GetUApi().GetInputByName("UAGorillaMapToggle");
        PlayerBase mapPlayer = PlayerBase.Cast(GetGame().GetPlayer());
        if (mapInput && mapInput.LocalPress() && mapPlayer && mapPlayer.IsAlive() && !mapPlayer.IsUnconscious())
        {
            if (!GetGame().GetUIManager().GetMenu())
                GetGame().GetUIManager().EnterScriptedMenu(784193, null);
        }
        super.OnUpdate(timeslice);
    }
};
