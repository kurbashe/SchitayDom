// Uses the engine's global RPC event rather than modifying a lower script module.
// Official precedents: SyncEvents.RegisterEvents and ScriptConsoleGeneralTab's
// subscription to BunkerBroadcastHandler.OnRPC.
class GMapRPC
{
    static void Init()
    {
        DayZGame game = DayZGame.Cast(GetGame());
        if (!game) return;
        // Missions may restart while DayZGame survives. Never accumulate handlers.
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
        // This protocol intentionally uses global messages only. Other IDs and
        // entity-targeted RPC buffers remain untouched for the engine and mods.
        if (target || !GetGame()) return;
        if (rpc_type == GMapProtocol.REQUEST)
        {
            if (GetGame().IsServer()) GMapService.Get().Handle(sender, ctx);
            return;
        }
        if (rpc_type == GMapProtocol.RESPONSE)
        {
            if (GetGame().IsClient()) GMapClient.Receive(ctx);
        }
    }
}
