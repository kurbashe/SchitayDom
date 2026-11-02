// The shared/client addon never processes client requests or opens server state.
class GMapClientRPC
{
    static void Init()
    {
        if (!GetGame() || !GetGame().IsClient()) return;
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
        // Do not read another listener's buffer. Only accept server responses.
        if (target || rpc_type != GMapProtocol.RESPONSE || !GetGame() || !GetGame().IsClient()) return;
        GMapClient.Receive(ctx);
    }
};
