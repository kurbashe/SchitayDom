// Installed only on the server through -serverMod=@GorillaMapServer.
class GMapServerRPC
{
    static void Init()
    {
        if (!GetGame() || !GetGame().IsServer()) return;
        DayZGame game = DayZGame.Cast(GetGame());
        if (!game) return;
        game.Event_OnRPC.Remove(OnRPC);
        game.Event_OnRPC.Insert(OnRPC);
    }

    static void Shutdown()
    {
        DayZGame game = DayZGame.Cast(GetGame());
        if (game) game.Event_OnRPC.Remove(OnRPC);
    }

    static void OnRPC(PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx)
    {
        // Client code cannot choose an identity: Handle uses engine sender.
        if (target || rpc_type != GMapProtocol.REQUEST || !GetGame() || !GetGame().IsServer()) return;
        GMapService.Get().Handle(sender, ctx);
    }
};
