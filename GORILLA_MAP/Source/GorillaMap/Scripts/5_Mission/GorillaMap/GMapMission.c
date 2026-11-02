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
        Print("[GORILLA MAP 0.1.3] Client initialized; open action=UAGorillaMapOpen (default F6)");
    }

    override void OnMissionFinish()
    {
        GMapClientRPC.Shutdown();
        GMapClient.Reset();
        super.OnMissionFinish();
    }

    override void OnUpdate(float timeslice)
    {
        // Separate action/default key from the vanilla and third-party maps on M.
        UAInput mapInput = GetUApi().GetInputByName("UAGorillaMapOpen");
        PlayerBase mapPlayer = PlayerBase.Cast(GetGame().GetPlayer());
        if (mapInput && mapInput.LocalPress() && mapPlayer && mapPlayer.IsAlive() && !mapPlayer.IsUnconscious())
        {
            if (!GetGame().GetUIManager().GetMenu())
            {
                UIScriptedMenu opened = GetGame().GetUIManager().EnterScriptedMenu(784193, null);
                if (!GMapMenu.Cast(opened))
                    Print("[GORILLA MAP 0.1.3] ERROR: menu factory did not create GMapMenu");
            }
        }
        super.OnUpdate(timeslice);
    }
};
