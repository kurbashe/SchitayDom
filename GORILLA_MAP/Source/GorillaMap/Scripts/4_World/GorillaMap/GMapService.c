class GMapInvite
{
    string GroupId;
    string From;
    int Expires;
}

class GMapService
{
    protected static ref GMapService s_Instance;
    protected ref GMapConfig m_Config;
    protected ref GMapStore m_Store;
    protected ref map<string, int> m_ReadRate = new map<string, int>;
    protected ref map<string, int> m_WriteRate = new map<string, int>;
    protected ref map<string, ref GMapInvite> m_Invites = new map<string, ref GMapInvite>;
    protected bool m_Initialized;
    protected bool m_Ready;
    protected string m_Failure;
    static const string ROOT = "$profile:GorillaMap";
    static const string CONFIG_PATH = "$profile:GorillaMap/Config.json";
    static const string STATE_PATH = "$profile:GorillaMap/State.json";

    static GMapService Get()
    {
        if (!s_Instance) s_Instance = new GMapService;
        return s_Instance;
    }

    static void Reset()
    {
        s_Instance = null;
    }

    void Init()
    {
        if (m_Initialized || !GetGame().IsServer()) return;
        m_Initialized = true;
        MakeDirectory(ROOT);
        string error;
        if (FileExist(CONFIG_PATH))
        {
            if (!JsonFileLoader<GMapConfig>.LoadFile(CONFIG_PATH, m_Config, error))
            {
                Fail("Config.json: " + error);
                return;
            }
        }
        else
        {
            m_Config = new GMapConfig;
            if (!JsonFileLoader<GMapConfig>.SaveFile(CONFIG_PATH, m_Config, error))
            {
                Fail("Cannot create Config.json: " + error);
                return;
            }
        }
        if (!ValidateConfig())
        {
            Fail("Config.json validation failed (version, world, limits or markers). File preserved.");
            return;
        }
        if (FileExist(STATE_PATH))
        {
            if (!JsonFileLoader<GMapStore>.LoadFile(STATE_PATH, m_Store, error) || !ValidateStore())
            {
                Fail("State.json invalid or unreadable. No reset: restore a verified backup. " + error);
                return;
            }
        }
        else
        {
            // Never silently replace a state lost during an interrupted write.
            if (FileExist(STATE_PATH + ".bak") || FileExist(STATE_PATH + ".pending"))
            {
                Fail("State.json missing while backup/pending exists; administrator recovery required.");
                return;
            }
            m_Store = new GMapStore;
            if (!Persist()) return;
        }
        m_Ready = true;
        Print("[GORILLA MAP] Ready; protocol 1; world=" + GetGame().GetWorldName());
    }

    protected void Fail(string reason)
    {
        m_Ready = false;
        m_Failure = reason;
        Print("[GORILLA MAP] DISABLED: " + reason);
    }

    protected bool ValidPosition(vector position)
    {
        // Positive range comparisons also reject NaN and infinity.
        return position[0] >= 0 && position[0] <= m_Config.WorldSize && position[2] >= 0 && position[2] <= m_Config.WorldSize;
    }

    protected bool ValidIcon(string icon)
    {
        return icon == "pin" || icon == "home" || icon == "skull" || icon == "star" || icon == "water" || icon == "medical" || icon == "camp" || icon == "radiation" || icon == "prohibited";
    }

    protected string CleanName(string name)
    {
        name.Replace("\n", " ");
        name.Replace("\r", " ");
        name.Replace("\t", " ");
        name.TrimInPlace();
        return name;
    }

    protected bool ValidName(string name)
    {
        return name.LengthUtf8() > 0 && name.LengthUtf8() <= 48 && name.IndexOf("\n") == -1 && name.IndexOf("\r") == -1 && name.IndexOf("\t") == -1;
    }

    protected bool ValidateMarker(GMapMarker marker)
    {
        if (!marker || !ValidName(marker.Name) || marker.Id == "" || marker.Id.Length() > 80) return false;
        if (!ValidPosition(marker.Position) || !ValidIcon(marker.Icon)) return false;
        return marker.Radius >= 0 && marker.Radius <= m_Config.WorldSize;
    }

    protected bool ValidateConfig()
    {
        if (!m_Config || m_Config.Version != 1 || !m_Config.Markers) return false;
        string world = GetGame().GetWorldName();
        world.ToLower();
        string configured = m_Config.WorldName;
        configured.ToLower();
        if (world != configured) return false;
        if (!(m_Config.WorldSize >= 1000 && m_Config.WorldSize <= 100000)) return false;
        if (m_Config.MaxPersonalMarkers < 1 || m_Config.MaxPersonalMarkers > 200) return false;
        if (m_Config.MaxGroupMarkers < 1 || m_Config.MaxGroupMarkers > 200) return false;
        if (m_Config.MaxGroupMembers < 2 || m_Config.MaxGroupMembers > 32) return false;
        if (m_Config.MaxGroups < 1 || m_Config.MaxGroups > 10000 || m_Config.MaxAccounts < 1 || m_Config.MaxAccounts > 100000) return false;
        if (m_Config.Markers.Count() > 200) return false;
        ref map<string, bool> ids = new map<string, bool>;
        foreach (GMapMarker marker : m_Config.Markers)
        {
            if (!ValidateMarker(marker) || marker.Kind < GMapKind.SERVER || marker.Kind > GMapKind.RESTRICTED || ids.Contains(marker.Id)) return false;
            ids.Set(marker.Id, true);
        }
        return true;
    }

    protected bool ValidStoredId(string id, string prefix)
    {
        if (id.IndexOf(prefix) != 0 || id.Length() <= prefix.Length()) return false;
        string suffix = id.Substring(prefix.Length(), id.Length() - prefix.Length());
        int number = suffix.ToInt();
        return number > 0 && number < m_Store.NextId && number.ToString() == suffix;
    }

    protected bool ValidateStore()
    {
        if (!m_Store || m_Store.Version != 1 || m_Store.NextId < 1 || m_Store.NextId > 2000000000) return false;
        if (!m_Store.Accounts || !m_Store.Groups || m_Store.Accounts.Count() > m_Config.MaxAccounts || m_Store.Groups.Count() > m_Config.MaxGroups) return false;
        ref map<string, bool> accounts = new map<string, bool>;
        ref map<string, bool> ids = new map<string, bool>;
        ref map<string, bool> members = new map<string, bool>;
        foreach (GMapAccount account : m_Store.Accounts)
        {
            if (!account || account.Uid == "" || account.Uid.Length() > 128 || !account.Markers || account.Markers.Count() > m_Config.MaxPersonalMarkers || accounts.Contains(account.Uid)) return false;
            accounts.Set(account.Uid, true);
            foreach (GMapMarker personal : account.Markers)
            {
                if (!ValidateMarker(personal) || personal.Kind != GMapKind.PERSONAL || !ValidStoredId(personal.Id, "marker_") || personal.OwnerUid != account.Uid || ids.Contains(personal.Id)) return false;
                ids.Set(personal.Id, true);
            }
        }
        foreach (GMapGroup group : m_Store.Groups)
        {
            if (!group || !ValidStoredId(group.Id, "group_") || ids.Contains(group.Id) || !ValidName(group.Name) || !group.Members || !group.Markers) return false;
            if (group.Members.Count() < 1 || group.Members.Count() > m_Config.MaxGroupMembers || group.Markers.Count() > m_Config.MaxGroupMarkers || group.Members.Find(group.LeaderUid) == -1) return false;
            ids.Set(group.Id, true);
            foreach (string uid : group.Members)
            {
                if (!accounts.Contains(uid) || members.Contains(uid)) return false;
                members.Set(uid, true);
            }
            foreach (GMapMarker shared : group.Markers)
            {
                if (!ValidateMarker(shared) || shared.Kind != GMapKind.GROUP || !ValidStoredId(shared.Id, "marker_") || !accounts.Contains(shared.OwnerUid) || ids.Contains(shared.Id)) return false;
                ids.Set(shared.Id, true);
            }
        }
        return true;
    }

    // Checked staging copy + previous snapshot. Not an atomic filesystem transaction.
    // Any write/verification failure disables mutations until explicit administrator recovery.
    protected bool Persist()
    {
        string error;
        string expected;
        if (!JsonFileLoader<GMapStore>.MakeData(m_Store, expected, error, false))
        {
            Fail("Cannot serialize state: " + error);
            return false;
        }
        string pending = STATE_PATH + ".pending";
        if (!JsonFileLoader<GMapStore>.SaveFile(pending, m_Store, error))
        {
            Fail("Cannot write pending state: " + error);
            return false;
        }
        GMapStore verified;
        string actual;
        if (!JsonFileLoader<GMapStore>.LoadFile(pending, verified, error) || !JsonFileLoader<GMapStore>.MakeData(verified, actual, error, false) || actual != expected)
        {
            Fail("Pending state verification failed. Original preserved.");
            return false;
        }
        if (FileExist(STATE_PATH) && !CopyFile(STATE_PATH, STATE_PATH + ".bak"))
        {
            Fail("Cannot back up previous state. Original preserved.");
            return false;
        }
        if (!CopyFile(pending, STATE_PATH))
        {
            Fail("Cannot promote pending state. Recover from verified backup.");
            return false;
        }
        if (!JsonFileLoader<GMapStore>.LoadFile(STATE_PATH, verified, error) || !JsonFileLoader<GMapStore>.MakeData(verified, actual, error, false) || actual != expected)
        {
            Fail("Final state verification failed. Recover from backup/pending.");
            return false;
        }
        DeleteFile(pending);
        return true;
    }

    protected GMapAccount Account(string uid, bool create = false, string name = "")
    {
        foreach (GMapAccount account : m_Store.Accounts)
            if (account.Uid == uid) return account;
        if (!create || m_Store.Accounts.Count() >= m_Config.MaxAccounts) return null;
        GMapAccount fresh = new GMapAccount;
        fresh.Uid = uid;
        fresh.Name = name;
        m_Store.Accounts.Insert(fresh);
        return fresh;
    }

    protected GMapGroup GroupFor(string uid)
    {
        foreach (GMapGroup group : m_Store.Groups)
            if (group.Members.Find(uid) != -1) return group;
        return null;
    }

    protected GMapGroup GroupById(string id)
    {
        foreach (GMapGroup group : m_Store.Groups)
            if (group.Id == id) return group;
        return null;
    }

    protected GMapInvite Invitation(string uid)
    {
        GMapInvite invite;
        if (!m_Invites.Find(uid, invite)) return null;
        if (GetGame().GetTime() > invite.Expires || !GroupById(invite.GroupId))
        {
            m_Invites.Remove(uid);
            return null;
        }
        return invite;
    }

    protected bool Allow(string uid, bool snapshot)
    {
        int last;
        int now = GetGame().GetTime();
        map<string, int> rate = m_WriteRate;
        int delay = 600;
        if (snapshot)
        {
            rate = m_ReadRate;
            delay = 1500;
        }
        if (rate.Find(uid, last) && now >= last && now - last < delay) return false;
        rate.Set(uid, now);
        return true;
    }

    protected string NewId(string prefix)
    {
        if (m_Store.NextId >= 2000000000) return "";
        string id = prefix + m_Store.NextId.ToString();
        m_Store.NextId++;
        return id;
    }

    void Handle(PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!GetGame().IsServer() || !sender) return;
        PlayerBase player = PlayerBase.Cast(sender.GetPlayer());
        if (!player || !player.IsAlive()) return;
        int version;
        string command;
        if (!ctx.Read(version) || version != GMapProtocol.VERSION || !ctx.Read(command) || command.Length() > 32) return;
        string uid = sender.GetId();
        if (uid == "" || !Allow(uid, command == "snapshot")) return;
        string id;
        string name;
        vector position;
        int color;
        string icon;
        bool shared;
        if (!ctx.Read(id) || !ctx.Read(name) || !ctx.Read(position) || !ctx.Read(color) || !ctx.Read(icon) || !ctx.Read(shared)) return;
        if (id.Length() > 128 || name.Length() > 192 || icon.Length() > 20) return;
        Init();
        if (!m_Ready)
        {
            Reply(sender, "Карта временно недоступна: проверьте серверный журнал GORILLA MAP.");
            return;
        }
        name = CleanName(name);
        string message;
        bool changed;
        if (command == "snapshot")
        {
            Reply(sender, "");
            return;
        }
        string before;
        string serializationError;
        if (!JsonFileLoader<GMapStore>.MakeData(m_Store, before, serializationError, false))
        {
            Fail("Cannot prepare state mutation: " + serializationError);
            Reply(sender, "Ошибка состояния; изменения отключены.");
            return;
        }
        if (command == "add" || command == "update" || command == "delete")
            changed = EditMarker(uid, sender.GetName(), command, id, name, position, color, icon, shared, message);
        else if (command == "group_create" || command == "group_invite" || command == "group_accept" || command == "group_decline" || command == "group_leave" || command == "group_kick")
            changed = EditGroup(uid, sender.GetName(), command, id, name, message);
        else
            message = "Неизвестная команда.";
        bool saved = true;
        if (changed) saved = Persist();
        if (!changed || !saved)
        {
            GMapStore restored;
            if (JsonFileLoader<GMapStore>.LoadData(before, restored, serializationError)) m_Store = restored;
            else Fail("In-memory rollback failed.");
        }
        if (!saved) message = "Ошибка сохранения. Изменения отключены; обратитесь к администратору.";
        Reply(sender, message);
    }

    protected bool EditMarker(string uid, string playerName, string command, string id, string name, vector position, int color, string icon, bool shared, out string message)
    {
        GMapGroup group = GroupFor(uid);
        GMapAccount account = Account(uid, command == "add", playerName);
        if (!account)
        {
            message = "Профиль недоступен или лимит профилей исчерпан.";
            return false;
        }
        if (shared && !group)
        {
            message = "Для общей метки вступите в группу.";
            return false;
        }
        array<ref GMapMarker> markers = account.Markers;
        int maxMarkers = m_Config.MaxPersonalMarkers;
        if (shared)
        {
            markers = group.Markers;
            maxMarkers = m_Config.MaxGroupMarkers;
        }
        if (command != "delete" && (!ValidName(name) || !ValidPosition(position) || !ValidIcon(icon)))
        {
            message = "Некорректное название, координаты или значок.";
            return false;
        }
        GMapMarker marker;
        int index = -1;
        if (command == "add")
        {
            if (markers.Count() >= maxMarkers)
            {
                message = "Достигнут лимит меток.";
                return false;
            }
            string markerId = NewId("marker_");
            if (markerId == "")
            {
                message = "Лимит идентификаторов; обратитесь к администратору.";
                return false;
            }
            marker = new GMapMarker;
            marker.Id = markerId;
            marker.OwnerUid = uid;
            marker.Kind = GMapKind.PERSONAL;
            if (shared) marker.Kind = GMapKind.GROUP;
            markers.Insert(marker);
        }
        else
        {
            for (int i = 0; i < markers.Count(); i++)
            {
                if (markers[i].Id == id)
                {
                    marker = markers[i];
                    index = i;
                    break;
                }
            }
            if (!marker || (marker.OwnerUid != uid && (!group || !shared || group.LeaderUid != uid)))
            {
                message = "Метка не найдена или недостаточно прав.";
                return false;
            }
        }
        if (command == "delete")
        {
            markers.Remove(index);
            message = "Метка удалена.";
            return true;
        }
        position[1] = GetGame().SurfaceY(position[0], position[2]);
        marker.Name = name;
        marker.Position = position;
        marker.Color = color | 0xFF000000;
        marker.Icon = icon;
        account.Name = playerName;
        message = "Метка сохранена.";
        return true;
    }

    protected bool EditGroup(string uid, string playerName, string command, string id, string name, out string message)
    {
        GMapGroup group = GroupFor(uid);
        if (command == "group_create")
        {
            if (group || !ValidName(name) || m_Store.Groups.Count() >= m_Config.MaxGroups)
            {
                message = "Уже есть группа, неверное название или достигнут лимит групп.";
                return false;
            }
            GMapAccount account = Account(uid, true, playerName);
            if (!account)
            {
                message = "Лимит профилей исчерпан.";
                return false;
            }
            string groupId = NewId("group_");
            if (groupId == "") return false;
            GMapGroup created = new GMapGroup;
            created.Id = groupId;
            created.Name = name;
            created.LeaderUid = uid;
            created.Members.Insert(uid);
            m_Store.Groups.Insert(created);
            m_Invites.Remove(uid);
            message = "Группа создана.";
            return true;
        }
        if (command == "group_accept")
        {
            GMapInvite invite = Invitation(uid);
            if (group || !invite)
            {
                message = "Приглашение недействительно или вы уже в группе.";
                return false;
            }
            GMapGroup joining = GroupById(invite.GroupId);
            if (!joining || joining.Members.Count() >= m_Config.MaxGroupMembers || !Account(uid, true, playerName))
            {
                message = "В группе нет места или профиль недоступен.";
                return false;
            }
            joining.Members.Insert(uid);
            m_Invites.Remove(uid);
            message = "Вы вступили в группу.";
            return true;
        }
        if (command == "group_decline")
        {
            m_Invites.Remove(uid);
            message = "Приглашение отклонено.";
            return false;
        }
        if (!group)
        {
            message = "Вы не состоите в группе.";
            return false;
        }
        if (command == "group_leave")
        {
            if (group.LeaderUid == uid)
            {
                m_Store.Groups.RemoveItem(group);
                message = "Группа распущена, её общие метки удалены.";
            }
            else
            {
                group.Members.RemoveItem(uid);
                message = "Вы покинули группу.";
            }
            return true;
        }
        if (group.LeaderUid != uid)
        {
            message = "Действие доступно только лидеру.";
            return false;
        }
        if (command == "group_kick")
        {
            if (id == uid || group.Members.Find(id) == -1)
            {
                message = "Участник не найден.";
                return false;
            }
            group.Members.RemoveItem(id);
            message = "Участник исключён.";
            return true;
        }
        if (command == "group_invite")
        {
            if (id == uid || group.Members.Count() >= m_Config.MaxGroupMembers || GroupFor(id))
            {
                message = "Игрок уже в группе или достигнут лимит участников.";
                return false;
            }
            ref array<Man> players = new array<Man>;
            GetGame().GetPlayers(players);
            bool online;
            foreach (Man target : players)
            {
                if (target && target.GetIdentity() && target.GetIdentity().GetId() == id) online = true;
            }
            if (!online)
            {
                message = "Игрок должен быть на сервере.";
                return false;
            }
            GMapInvite invitation = new GMapInvite;
            invitation.GroupId = group.Id;
            invitation.From = group.Name + " / " + playerName;
            invitation.Expires = GetGame().GetTime() + 600000;
            m_Invites.Set(id, invitation);
            message = "Приглашение отправлено на 10 минут. Игрок увидит его на вкладке группы.";
            return false;
        }
        message = "Неизвестное действие группы.";
        return false;
    }

    protected void Reply(PlayerIdentity recipient, string message)
    {
        GMapSnapshot snapshot = new GMapSnapshot;
        snapshot.Message = message;
        snapshot.MyUid = recipient.GetId();
        snapshot.Ready = m_Ready;
        if (m_Ready)
        {
            snapshot.WorldName = m_Config.WorldName;
            snapshot.ShowPlayerPosition = m_Config.ShowPlayerPosition;
            snapshot.MaxPersonalMarkers = m_Config.MaxPersonalMarkers;
            snapshot.MaxGroupMarkers = m_Config.MaxGroupMarkers;
            foreach (GMapMarker serverMarker : m_Config.Markers) snapshot.Markers.Insert(serverMarker);
            GMapAccount account = Account(snapshot.MyUid);
            if (account)
                foreach (GMapMarker personal : account.Markers) snapshot.Markers.Insert(personal);
            GMapGroup group = GroupFor(snapshot.MyUid);
            if (group)
            {
                snapshot.GroupId = group.Id;
                snapshot.GroupName = group.Name;
                snapshot.IsLeader = group.LeaderUid == snapshot.MyUid;
                foreach (GMapMarker shared : group.Markers) snapshot.Markers.Insert(shared);
                foreach (string uid : group.Members)
                {
                    GMapAccount member = Account(uid);
                    GMapPeer peer = new GMapPeer;
                    peer.Uid = uid;
                    peer.Name = uid;
                    if (member) peer.Name = member.Name;
                    snapshot.GroupMembers.Insert(peer);
                }
            }
            GMapInvite invitation = Invitation(snapshot.MyUid);
            if (invitation) snapshot.InviteFrom = invitation.From;
            ref array<Man> players = new array<Man>;
            GetGame().GetPlayers(players);
            foreach (Man player : players)
            {
                if (!player || !player.GetIdentity()) continue;
                GMapPeer online = new GMapPeer;
                online.Uid = player.GetIdentity().GetId();
                online.Name = player.GetIdentity().GetName();
                snapshot.OnlinePlayers.Insert(online);
            }
        }
        string json;
        string error;
        if (!JsonFileLoader<GMapSnapshot>.MakeData(snapshot, json, error, false)) return;
        ScriptRPC rpc = new ScriptRPC;
        rpc.Write(GMapProtocol.VERSION);
        rpc.Write(json);
        rpc.Send(null, GMapProtocol.RESPONSE, true, recipient);
    }
}
