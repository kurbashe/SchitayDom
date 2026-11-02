class GMapMenu extends UIScriptedMenu
{
    protected MapWidget m_Map;
    protected CanvasWidget m_Canvas;
    protected ref GMapPreferences m_Prefs;
    protected ref array<ref GMapRow> m_Rows = new array<ref GMapRow>;
    protected ref array<string> m_Icons = {"pin", "home", "skull", "star", "water", "medical", "camp", "radiation", "prohibited"};
    protected ref array<int> m_Colors = {0xFF00D99B, 0xFF21A4FF, 0xFFFFC83D, 0xFFFF535F, 0xFFBF78FF, 0xFFFFFFFF};
    protected int m_LastRevision = -1;
    protected int m_IconIndex;
    protected int m_ColorIndex;
    protected int m_InviteIndex;
    protected string m_InviteUid;
    protected string m_RenderKey;
    protected bool m_InitialCenter;
    protected string m_EditId;
    protected string m_SelectedId;
    protected bool m_Shared;
    protected bool m_ConfirmDelete;
    protected bool m_ConfirmLeave;
    protected bool m_Ruler;
    protected bool m_HasRulerStart;
    protected bool m_HasRulerEnd;
    protected vector m_RulerStart;
    protected vector m_RulerEnd;
    protected vector m_CursorWorld;
    protected float m_Poll;
    protected float m_Draw;
    protected float m_OpenTime;
    protected ref GMapMapHandler m_MapHandler;
    protected IngameHud m_Hud;
    protected bool m_HudHidden;
    protected bool m_PreviousMenuOpen;
    protected bool m_PreviousQuickbarDisabled;
    protected bool m_Shown;
    protected bool m_CloseRequested;
    protected bool m_LayoutReady;
    protected int m_LastBackTime = -1000;

    override Widget Init()
    {
        layoutRoot = GetGame().GetWorkspace().CreateWidgets("GorillaMap/GUI/layouts/main.layout");
        if (!layoutRoot)
        {
            Print("[GORILLA MAP 0.1.3] ERROR: main.layout could not be created");
            return null;
        }
        m_Map = MapWidget.Cast(W("Map"));
        m_Canvas = CanvasWidget.Cast(W("ZoneCanvas"));
        m_LayoutReady = m_Map && m_Canvas && W("Sidebar") && W("Editor") && W("CreateMarker") && W("CloseMap");
        layoutRoot.SetSort(1000);
        if (m_Map) m_Map.SetSort(0);
        if (m_Canvas) m_Canvas.SetSort(10);
        if (W("PlayerLabel")) W("PlayerLabel").SetSort(20);
        if (W("Sidebar")) W("Sidebar").SetSort(100);
        if (W("InfoCard")) W("InfoCard").SetSort(110);
        if (W("WorldBadge")) W("WorldBadge").SetSort(110);
        if (W("Compass")) W("Compass").SetSort(110);
        if (W("ExpandSidebarFrame")) W("ExpandSidebarFrame").SetSort(120);
        if (W("Editor")) W("Editor").SetSort(300);
        if (W("Groups")) W("Groups").SetSort(300);
        if (W("Settings")) W("Settings").SetSort(300);
        Show("Sidebar", true);
        Show("ExpandSidebar", false);
        Show("Editor", false);
        Show("Groups", false);
        Show("Settings", false);
        m_Prefs = GMapPreferences.Load();
        m_InitialCenter = GMapClient.Revision == 0;
        if (m_Map)
        {
            m_Map.SetScale(m_Prefs.Scale);
            m_Map.SetMapPos(m_Prefs.Center);
        }
        SetText("Status", "Подключение к серверу…");
        RefreshSettings();
        GMapClient.Send("snapshot");
        return layoutRoot;
    }

    override void OnShow()
    {
        if (m_Shown) return;
        super.OnShow();
        m_Shown = true;
        m_CloseRequested = false;
        if (!m_LayoutReady)
        {
            Print("[GORILLA MAP 0.1.3] ERROR: required UI widgets are missing; closing instead of locking input");
            RequestClose();
            return;
        }
        m_MapHandler = new GMapMapHandler(this);
        m_Map.SetHandler(m_MapHandler);
        BindControlHandlers(layoutRoot, m_MapHandler);
        SetFocus(m_Map);
        if (GetGame().GetMission())
            m_Hud = IngameHud.Cast(GetGame().GetMission().GetHud());
        if (m_Hud && m_Hud.GetHudVisibility() && !m_HudHidden)
        {
            m_PreviousMenuOpen = m_Hud.GetHudVisibility().IsContextFlagActive(EHudContextFlags.MENU_OPEN);
            m_PreviousQuickbarDisabled = m_Hud.GetHudVisibility().IsContextFlagActive(EHudContextFlags.QUICKBAR_DISABLE);
            m_Hud.ShowHudUI(false);
            m_Hud.ShowQuickbarUI(false);
            m_HudHidden = true;
        }
        Print("[GORILLA MAP 0.1.3] OPEN: menu=GMapMenu sidebar=" + W("Sidebar").IsVisibleHierarchy().ToString() + " priority=" + W("Sidebar").GetSort().ToString());
    }

    protected void RestorePresentation()
    {
        if (m_HudHidden && m_Hud)
        {
            m_Hud.ShowHudUI(!m_PreviousMenuOpen);
            m_Hud.ShowQuickbarUI(!m_PreviousQuickbarDisabled);
        }
        m_HudHidden = false;
        if (m_Map) m_Map.SetHandler(null);
        if (layoutRoot) BindControlHandlers(layoutRoot, null);
        if (m_MapHandler) m_MapHandler.Detach();
        m_MapHandler = null;
    }

    protected void BindControlHandlers(Widget parent, ScriptedWidgetEventHandler handler)
    {
        Widget child = parent.GetChildren();
        while (child)
        {
            if (ButtonWidget.Cast(child) || EditBoxWidget.Cast(child)) child.SetHandler(handler);
            BindControlHandlers(child, handler);
            child = child.GetSibling();
        }
    }

    protected void RequestClose()
    {
        if (m_CloseRequested) return;
        m_CloseRequested = true;
        Close();
    }

    protected void HandleBack()
    {
        int now = GetGame().GetTime();
        if (now - m_LastBackTime < 200) return;
        m_LastBackTime = now;
        if (Visible("Editor")) Show("Editor", false);
        else if (Visible("Groups")) Show("Groups", false);
        else if (Visible("Settings")) Show("Settings", false);
        else RequestClose();
        if (!m_CloseRequested && m_Map) SetFocus(m_Map);
    }

    protected Widget W(string name)
    {
        if (!layoutRoot)
            return null;
        return layoutRoot.FindAnyWidget(name);
    }

    protected void SetText(string name, string text)
    {
        TextWidget label = TextWidget.Cast(W(name));
        if (label)
            label.SetText(text);
        ButtonWidget button = ButtonWidget.Cast(W(name));
        if (button)
            button.SetText(text);
    }

    protected void Show(string name, bool visible)
    {
        Widget widget = W(name + "Frame");
        if (!widget)
            widget = W(name);
        if (widget)
            widget.Show(visible);
    }

    protected bool Visible(string name)
    {
        Widget widget = W(name);
        return widget && widget.IsVisible();
    }

    protected bool Modal()
    {
        return Visible("Editor") || Visible("Groups") || Visible("Settings");
    }

    override void OnHide()
    {
        if (m_Map && m_Prefs)
        {
            m_Prefs.Center = m_Map.GetMapPos();
            m_Prefs.Scale = m_Map.GetScale();
            m_Prefs.Save();
        }
        RestorePresentation();
        // UIScriptedMenu owns input focus and the death subscription.
        if (m_Shown)
        {
            m_Shown = false;
            super.OnHide();
        }
        Print("[GORILLA MAP 0.1.3] CLOSE: HUD and input focus restored");
    }

    override void Update(float timeslice)
    {
        super.Update(timeslice);
        m_OpenTime += timeslice;
        // Read the UI back action even when the MapWidget or text field owns focus.
        if (m_OpenTime > 0.2 && GetUApi().GetInputByID(UAUIBack).LocalPress())
        {
            HandleBack();
            return;
        }
        if (m_CloseRequested) return;
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player || !player.IsAlive() || player.IsUnconscious())
        {
            RequestClose();
            return;
        }
        if (!m_LayoutReady)
        {
            RequestClose();
            return;
        }
        if (GMapClient.Revision != m_LastRevision)
        {
            m_LastRevision = GMapClient.Revision;
            RefreshSnapshot();
        }
        m_Poll += timeslice;
        if (m_Poll >= 3.0)
        {
            m_Poll = 0;
            GMapClient.Send("snapshot");
        }
        if (GMapClient.Revision == 0 && m_OpenTime > 10)
            SetText("Status", "Нет соединения с картой. Повторяем подключение…");
        m_Draw += timeslice;
        if (m_Draw >= 0.05)
        {
            m_Draw = 0;
            int mouseX, mouseY;
            GetMousePos(mouseX, mouseY);
            m_CursorWorld = m_Map.ScreenToMap(Vector(mouseX, mouseY, 0));
            SetText("Coordinates", CoordinateText(m_CursorWorld));
            DrawOverlay(player);
        }
        UAInput toggle = GetUApi().GetInputByName("UAGorillaMapOpen");
        if (m_OpenTime > 0.3 && toggle && toggle.LocalPress() && !Modal())
            RequestClose();
    }

    protected string CoordinateText(vector position)
    {
        return Math.Round(position[0]).ToString() + " / " + Math.Round(position[2]).ToString();
    }

    protected bool PositionValid(vector position)
    {
        return position[0] >= 0 && position[0] <= 15360 && position[2] >= 0 && position[2] <= 15360;
    }

    protected string Texture(string icon)
    {
        if (m_Icons.Find(icon) < 0)
            icon = "pin";
        return "GorillaMap/GUI/textures/marker_" + icon + "_ca.paa";
    }

    protected bool IsHidden(string id)
    {
        return m_Prefs.Hidden.Find(id) >= 0;
    }

    protected GMapMarker FindMarker(string id)
    {
        if (!GMapClient.State || !GMapClient.State.Markers)
            return null;
        foreach (GMapMarker marker : GMapClient.State.Markers)
        {
            if (marker && marker.Id == id)
                return marker;
        }
        return null;
    }

    protected bool Editable(GMapMarker marker)
    {
        if (!marker || !GMapClient.State)
            return false;
        if (marker.Kind == GMapKind.PERSONAL)
            return marker.OwnerUid == GMapClient.State.MyUid;
        if (marker.Kind == GMapKind.GROUP)
            return marker.OwnerUid == GMapClient.State.MyUid || GMapClient.State.IsLeader;
        return false;
    }

    protected string RowsFor(int kind)
    {
        switch (kind)
        {
            case GMapKind.SERVER: return "ServerRows";
            case GMapKind.ZONE: return "ZoneRows";
            case GMapKind.RESTRICTED: return "BuildRows";
            case GMapKind.PERSONAL: return "PersonalRows";
            case GMapKind.GROUP: return "GroupRows";
        }
        return "ServerRows";
    }

    protected void RefreshSnapshot()
    {
        if (!GMapClient.State)
            return;
        if (GMapClient.State.Ready && m_InitialCenter)
        {
            m_InitialCenter = false;
            CenterPlayer();
        }
        string renderKey = GMapClient.State.GroupId + GMapClient.State.IsLeader.ToString();
        foreach (GMapMarker keyMarker : GMapClient.State.Markers)
        {
            renderKey += keyMarker.Id + keyMarker.Name + keyMarker.Position.ToString() + keyMarker.Color.ToString() + keyMarker.Icon + keyMarker.Radius.ToString();
            if (IsHidden(keyMarker.Id)) renderKey += "hidden";
        }
        if (renderKey != m_RenderKey)
        {
            m_RenderKey = renderKey;
            RebuildRows();
            RefreshMapMarks();
        }
        RefreshGroups();
        if (GMapClient.State.Message != "")
            SetText("Status", GMapClient.State.Message);
        else if (GMapClient.Revision == 0)
            SetText("Status", "Подключение к серверу…");
        else if (GMapClient.State.InviteFrom != "")
            SetText("Status", "Приглашение в группу — откройте «Группа»");
        else
            SetText("Status", "GORILLA MAP  /  ЧЕРНОРУССИЯ");
    }

    protected void RebuildRows()
    {
        foreach (GMapRow oldRow : m_Rows)
        {
            if (oldRow.Root)
                oldRow.Root.Unlink();
        }
        m_Rows.Clear();
        int counts[5];
        foreach (GMapMarker marker : GMapClient.State.Markers)
        {
            if (!marker || marker.Kind < 0 || marker.Kind > 4)
                continue;
            counts[marker.Kind] = counts[marker.Kind] + 1;
            Widget parent = W(RowsFor(marker.Kind));
            if (!parent)
                continue;
            Widget rowWidget = GetGame().GetWorkspace().CreateWidgets("GorillaMap/GUI/layouts/marker_row.layout", parent);
            if (!rowWidget)
                continue;
            BindControlHandlers(rowWidget, m_MapHandler);
            GMapRow row = new GMapRow;
            row.Root = rowWidget;
            row.Marker = marker;
            row.Select = ButtonWidget.Cast(rowWidget.FindAnyWidget("RowSelect"));
            row.Visibility = ButtonWidget.Cast(rowWidget.FindAnyWidget("RowVisibility"));
            row.Edit = ButtonWidget.Cast(rowWidget.FindAnyWidget("RowEdit"));
            row.Delete = ButtonWidget.Cast(rowWidget.FindAnyWidget("RowDelete"));
            TextWidget title = TextWidget.Cast(rowWidget.FindAnyWidget("RowTitle"));
            if (title)
                title.SetText(marker.Name);
            ImageWidget icon = ImageWidget.Cast(rowWidget.FindAnyWidget("RowIcon"));
            if (icon)
            {
                icon.LoadImageFile(0, Texture(marker.Icon));
                icon.SetColor(marker.Color);
            }
            if (row.Edit)
                row.Edit.Show(Editable(marker));
            if (row.Delete)
                row.Delete.Show(Editable(marker));
            if (row.Visibility)
            {
                if (IsHidden(marker.Id))
                    row.Visibility.SetAlpha(0.3);
                else
                    row.Visibility.SetAlpha(1.0);
            }
            m_Rows.Insert(row);
        }
        SetText("ServerCount", counts[0].ToString());
        SetText("ZoneCount", counts[1].ToString());
        SetText("BuildCount", counts[2].ToString());
        SetText("PersonalCount", counts[3].ToString());
        SetText("GroupCount", counts[4].ToString());
        Reflow();
    }

    protected void Reflow()
    {
        GridSpacerWidget stack = GridSpacerWidget.Cast(W("SectionsStack"));
        for (int kind = 0; kind < 5; kind++)
        {
            GridSpacerWidget grid = GridSpacerWidget.Cast(W(RowsFor(kind)));
            if (grid)
                grid.Update();
        }
        if (stack)
            stack.Update();
    }

    protected void RefreshMapMarks()
    {
        if (!m_Map)
            return;
        m_Map.ClearUserMarks();
        if (!GMapClient.State)
            return;
        foreach (GMapMarker marker : GMapClient.State.Markers)
        {
            if (!marker || IsHidden(marker.Id))
                continue;
            string label;
            if (m_Prefs.ShowNames)
                label = marker.Name;
            m_Map.AddUserMark(marker.Position, label, marker.Color, Texture(marker.Icon));
        }
    }

    protected void DrawOverlay(PlayerBase player)
    {
        if (!m_Canvas)
            return;
        m_Canvas.Clear();
        Show("PlayerLabel", false);
        float originX, originY;
        m_Canvas.GetScreenPos(originX, originY);
        if (GMapClient.State && m_Prefs.ShowZones)
        {
            foreach (GMapMarker zone : GMapClient.State.Markers)
            {
                if (zone && zone.Radius > 0 && !IsHidden(zone.Id))
                    DrawCircle(zone.Position, zone.Radius, zone.Color, originX, originY);
            }
        }
        if (m_Prefs.ShowPlayer && GMapClient.State && GMapClient.State.Ready && GMapClient.State.ShowPlayerPosition)
        {
            vector playerScreen = m_Map.MapToScreen(player.GetPosition());
            float px = playerScreen[0] - originX;
            float py = playerScreen[1] - originY;
            float angle = player.GetOrientation()[0] * Math.DEG2RAD;
            float tipX = px + Math.Sin(angle) * 12;
            float tipY = py - Math.Cos(angle) * 12;
            float sideX = Math.Cos(angle) * 6;
            float sideY = Math.Sin(angle) * 6;
            int color = 0xFF00EBA8;
            m_Canvas.DrawLine(tipX, tipY, px + sideX, py + sideY, 3, color);
            m_Canvas.DrawLine(tipX, tipY, px - sideX, py - sideY, 3, color);
            m_Canvas.DrawLine(px + sideX, py + sideY, px - sideX, py - sideY, 3, color);
            Widget playerLabel = W("PlayerLabel");
            if (playerLabel)
            {
                float mapWidth, mapHeight;
                m_Canvas.GetScreenSize(mapWidth, mapHeight);
                if (px > 0 && px < mapWidth - 50 && py > 0 && py < mapHeight - 30)
                {
                    playerLabel.SetScreenPos(playerScreen[0] + 16, playerScreen[1] - 14);
                    playerLabel.Show(true);
                    string playerName = "Вы";
                    if (player.GetIdentity())
                        playerName = player.GetIdentity().GetName();
                    SetText("PlayerLabel", playerName);
                }
            }
        }
        if (m_Ruler && m_HasRulerStart)
        {
            vector end = m_CursorWorld;
            if (m_HasRulerEnd)
                end = m_RulerEnd;
            vector a = m_Map.MapToScreen(m_RulerStart);
            vector b = m_Map.MapToScreen(end);
            m_Canvas.DrawLine(a[0] - originX, a[1] - originY, b[0] - originX, b[1] - originY, 2, 0xFFFFD15C);
            vector difference = end - m_RulerStart;
            difference[1] = 0;
            SetText("ScaleText", "Линейка: " + Math.Round(difference.Length()).ToString() + " м");
        }
        else
            SetText("ScaleText", "Колесо — масштаб  /  ЛКМ — перемещение");
    }

    protected void DrawCircle(vector center, float radius, int color, float originX, float originY)
    {
        vector last = m_Map.MapToScreen(center + Vector(radius, 0, 0));
        for (int segment = 1; segment <= 64; segment++)
        {
            float angle = segment * Math.PI2 / 64.0;
            vector next = m_Map.MapToScreen(center + Vector(Math.Cos(angle) * radius, 0, Math.Sin(angle) * radius));
            m_Canvas.DrawLine(last[0] - originX, last[1] - originY, next[0] - originX, next[1] - originY, 2, color);
            last = next;
        }
    }

    protected void CenterPlayer()
    {
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (m_Map && player && GMapClient.State && GMapClient.State.Ready && GMapClient.State.ShowPlayerPosition)
            m_Map.SetMapPos(player.GetPosition());
    }

    protected void ToggleRows(string name)
    {
        Show(name, !Visible(name));
        Reflow();
    }

    protected void OpenEditor(vector position, GMapMarker marker = null)
    {
        if (!PositionValid(position))
            return;
        m_EditId = "";
        m_Shared = false;
        m_IconIndex = 0;
        m_ColorIndex = 0;
        m_ConfirmDelete = false;
        string name;
        if (marker)
        {
            if (!Editable(marker))
                return;
            m_EditId = marker.Id;
            position = marker.Position;
            name = marker.Name;
            m_Shared = marker.Kind == GMapKind.GROUP;
            m_IconIndex = Math.Max(0, m_Icons.Find(marker.Icon));
            m_ColorIndex = Math.Max(0, m_Colors.Find(marker.Color));
        }
        EditBoxWidget.Cast(W("EditorName")).SetText(name);
        EditBoxWidget.Cast(W("EditorX")).SetText(Math.Round(position[0]).ToString());
        EditBoxWidget.Cast(W("EditorZ")).SetText(Math.Round(position[2]).ToString());
        Show("EditorDelete", marker != null);
        SetText("EditorError", "");
        if (!GMapClient.State || !GMapClient.State.Ready)
            SetText("EditorError", "Нет ответа сервера. Сохранение пока недоступно.");
        SetText("EditorDelete", "Удалить метку");
        Show("Editor", true);
        SetFocus(W("EditorName"));
        RefreshEditor();
    }

    protected void RefreshEditor()
    {
        if (m_Shared)
            SetText("EditorScope", "Видимость: группа");
        else
            SetText("EditorScope", "Видимость: только я");
        SetText("EditorIcon", "Значок: " + IconTitle(m_Icons[m_IconIndex]));
        SetText("EditorColor", "Цвет: " + (m_ColorIndex + 1).ToString() + " / " + m_Colors.Count().ToString());
        Widget colorButton = W("EditorColor");
        if (colorButton)
            colorButton.SetColor(m_Colors[m_ColorIndex]);
    }

    protected string IconTitle(string icon)
    {
        switch (icon)
        {
            case "pin": return "точка";
            case "home": return "дом";
            case "skull": return "опасность";
            case "star": return "звезда";
            case "water": return "вода";
            case "medical": return "медицина";
            case "camp": return "лагерь";
            case "radiation": return "радиация";
            case "prohibited": return "запрет";
        }
        return "точка";
    }

    protected bool ParseCoordinate(string text, out float result)
    {
        if (text.Length() == 0 || text.Length() > 10)
            return false;
        int points;
        for (int i = 0; i < text.Length(); i++)
        {
            string ch = text.Get(i);
            if (ch == ".")
            {
                points++;
                if (points > 1)
                    return false;
            }
            else if (ch != "0" && ch != "1" && ch != "2" && ch != "3" && ch != "4" && ch != "5" && ch != "6" && ch != "7" && ch != "8" && ch != "9")
                return false;
        }
        if (text == ".")
            return false;
        result = text.ToFloat();
        return result >= 0 && result <= 15360;
    }

    protected void SaveEditor()
    {
        if (!GMapClient.State || !GMapClient.State.Ready)
        {
            SetText("EditorError", "Нет ответа сервера. Метка не сохранена.");
            return;
        }
        string name = EditBoxWidget.Cast(W("EditorName")).GetText();
        name.TrimInPlace();
        float x, z;
        if (name.LengthUtf8() < 1 || name.LengthUtf8() > 48)
        {
            SetText("EditorError", "Название: от 1 до 48 символов");
            return;
        }
        if (!ParseCoordinate(EditBoxWidget.Cast(W("EditorX")).GetText(), x) || !ParseCoordinate(EditBoxWidget.Cast(W("EditorZ")).GetText(), z))
        {
            SetText("EditorError", "Координаты: числа от 0 до 15360");
            return;
        }
        string command = "add";
        if (m_EditId != "")
            command = "update";
        GMapClient.Send(command, m_EditId, name, Vector(x, 0, z), m_Colors[m_ColorIndex], m_Icons[m_IconIndex], m_Shared);
        Show("Editor", false);
        m_Poll = 0;
        SetText("Status", "Запрос отправлен…");
    }

    protected void RefreshSettings()
    {
        string playerText = "Моя позиция: выкл.";
        string namesText = "Подписи: выкл.";
        string zonesText = "Границы зон: выкл.";
        if (m_Prefs.ShowPlayer) playerText = "Моя позиция: вкл.";
        if (m_Prefs.ShowNames) namesText = "Подписи: вкл.";
        if (m_Prefs.ShowZones) zonesText = "Границы зон: вкл.";
        SetText("SettingsPlayer", playerText);
        SetText("SettingsNames", namesText);
        SetText("SettingsZones", zonesText);
    }

    protected void RefreshGroups()
    {
        if (!GMapClient.State)
            return;
        string info = "Вы не состоите в группе";
        if (GMapClient.State.GroupId != "")
        {
            info = GMapClient.State.GroupName;
            if (GMapClient.State.IsLeader)
                info += "  /  лидер";
            foreach (GMapPeer member : GMapClient.State.GroupMembers)
                info += "\n" + member.Name;
        }
        SetText("GroupInfo", info);
        string notice = "Нет приглашений";
        if (GMapClient.State.InviteFrom != "") notice = "Приглашение: " + GMapClient.State.InviteFrom;
        if (GMapClient.State.Message != "") notice = GMapClient.State.Message;
        if (m_ConfirmLeave && GMapClient.State.IsLeader) notice = "Вы — лидер. Выход распустит группу и удалит её метки.";
        SetText("GroupInvitations", notice);
        string players;
        int shown;
        foreach (GMapPeer peer : GMapClient.State.OnlinePlayers)
        {
            if (shown >= 8)
                break;
            if (peer.Uid != GMapClient.State.MyUid)
            {
                players += peer.Name + "\n";
                shown++;
            }
        }
        SetText("GroupPlayers", players);
        RefreshInvitePlayer();
        Show("GroupAccept", GMapClient.State.InviteFrom != "");
        Show("GroupDecline", GMapClient.State.InviteFrom != "");
        Show("GroupCreate", GMapClient.State.GroupId == "");
        Show("GroupInvite", GMapClient.State.IsLeader);
        Show("GroupPickPlayer", GMapClient.State.IsLeader);
        Show("GroupLeave", GMapClient.State.GroupId != "");
    }

    protected void RefreshInvitePlayer()
    {
        m_InviteUid = "";
        if (!GMapClient.State)
            return;
        ref array<ref GMapPeer> available = new array<ref GMapPeer>;
        foreach (GMapPeer peer : GMapClient.State.OnlinePlayers)
        {
            if (peer.Uid != GMapClient.State.MyUid)
                available.Insert(peer);
        }
        if (available.Count() == 0)
        {
            SetText("GroupPickPlayer", "Нет игроков для приглашения");
            return;
        }
        m_InviteIndex = m_InviteIndex % available.Count();
        m_InviteUid = available[m_InviteIndex].Uid;
        SetText("GroupPickPlayer", "Игрок: " + available[m_InviteIndex].Name + "  ›");
    }

    protected GMapMarker NearestMarker(int x, int y)
    {
        if (!GMapClient.State)
            return null;
        GMapMarker nearest;
        float distance = 24 * 24;
        foreach (GMapMarker marker : GMapClient.State.Markers)
        {
            if (!marker || IsHidden(marker.Id))
                continue;
            vector screen = m_Map.MapToScreen(marker.Position);
            float dx = screen[0] - x;
            float dy = screen[1] - y;
            float squared = dx * dx + dy * dy;
            if (squared < distance)
            {
                nearest = marker;
                distance = squared;
            }
        }
        return nearest;
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        string action = w.GetName();
        // Buttons consume their own events; MapWidget retains native drag/zoom.
        switch (action)
        {
            case "CloseMap": RequestClose(); return true;
            case "CollapseSidebar": Show("Sidebar", false); Show("ExpandSidebar", true); return true;
            case "ExpandSidebar": Show("Sidebar", true); Show("ExpandSidebar", false); return true;
            case "ToggleServer": ToggleRows("ServerRows"); return true;
            case "ToggleZones": ToggleRows("ZoneRows"); return true;
            case "ToggleBuild": ToggleRows("BuildRows"); return true;
            case "TogglePersonal": ToggleRows("PersonalRows"); return true;
            case "ToggleGroup": ToggleRows("GroupRows"); return true;
            case "CenterPlayer": CenterPlayer(); return true;
            case "CreateMarker": OpenEditor(m_Map.GetMapPos()); return true;
            case "OpenGroups": m_ConfirmLeave = false; SetText("GroupLeave", "Покинуть группу"); Show("Groups", true); RefreshGroups(); return true;
            case "GroupClose": Show("Groups", false); return true;
            case "OpenSettings": Show("Settings", true); return true;
            case "SettingsClose": Show("Settings", false); m_Prefs.Save(); return true;
            case "SettingsPlayer": m_Prefs.ShowPlayer = !m_Prefs.ShowPlayer; RefreshSettings(); return true;
            case "SettingsNames": m_Prefs.ShowNames = !m_Prefs.ShowNames; RefreshSettings(); RefreshMapMarks(); return true;
            case "SettingsZones": m_Prefs.ShowZones = !m_Prefs.ShowZones; RefreshSettings(); return true;
            case "EditorCancel": Show("Editor", false); return true;
            case "EditorSave": SaveEditor(); return true;
            case "EditorIcon": m_IconIndex = (m_IconIndex + 1) % m_Icons.Count(); RefreshEditor(); return true;
            case "EditorColor": m_ColorIndex = (m_ColorIndex + 1) % m_Colors.Count(); RefreshEditor(); return true;
            case "EditorScope":
                if (m_EditId == "" && GMapClient.State && GMapClient.State.GroupId != "")
                    m_Shared = !m_Shared;
                RefreshEditor();
                return true;
            case "EditorDelete":
                if (!m_ConfirmDelete)
                {
                    m_ConfirmDelete = true;
                    SetText("EditorDelete", "Удалить? Нажмите ещё раз");
                }
                else
                {
                    GMapClient.Send("delete", m_EditId, "", "0 0 0", 0, "pin", m_Shared);
                    Show("Editor", false);
                }
                return true;
            case "GroupCreate": GMapClient.Send("group_create", "", EditBoxWidget.Cast(W("GroupNameInput")).GetText()); return true;
            case "GroupPickPlayer": m_InviteIndex++; RefreshInvitePlayer(); return true;
            case "GroupInvite": if (m_InviteUid != "") GMapClient.Send("group_invite", m_InviteUid); return true;
            case "GroupAccept": GMapClient.Send("group_accept"); return true;
            case "GroupDecline": GMapClient.Send("group_decline"); return true;
            case "GroupLeave":
                if (!m_ConfirmLeave)
                {
                    m_ConfirmLeave = true;
                    SetText("GroupLeave", "Выйти? Нажмите ещё раз");
                    RefreshGroups();
                }
                else
                {
                    GMapClient.Send("group_leave");
                    m_ConfirmLeave = false;
                }
                return true;
        }
        if (Modal())
            return super.OnClick(w, x, y, button);
        foreach (GMapRow row : m_Rows)
        {
            if (w == row.Visibility)
            {
                int index = m_Prefs.Hidden.Find(row.Marker.Id);
                if (index < 0) m_Prefs.Hidden.Insert(row.Marker.Id);
                else m_Prefs.Hidden.Remove(index);
                RefreshSnapshot();
                return true;
            }
            if (w == row.Edit || w == row.Delete)
            {
                OpenEditor(row.Marker.Position, row.Marker);
                return true;
            }
            if (w == row.Select)
            {
                m_Map.SetMapPos(row.Marker.Position);
                m_SelectedId = row.Marker.Id;
                SetText("Status", row.Marker.Name + "  /  " + CoordinateText(row.Marker.Position));
                return true;
            }
        }
        if (w == m_Map)
        {
            vector point = m_Map.ScreenToMap(Vector(x, y, 0));
            if (m_Ruler && button == MouseState.LEFT)
            {
                if (!m_HasRulerStart || m_HasRulerEnd)
                {
                    m_RulerStart = point;
                    m_HasRulerStart = true;
                    m_HasRulerEnd = false;
                }
                else
                {
                    m_RulerEnd = point;
                    m_HasRulerEnd = true;
                }
                return true;
            }
            GMapMarker hit = NearestMarker(x, y);
            if (hit)
            {
                m_SelectedId = hit.Id;
                SetText("Status", hit.Name + "  /  " + CoordinateText(hit.Position));
                if (button == MouseState.RIGHT && Editable(hit))
                    OpenEditor(hit.Position, hit);
            }
        }
        return super.OnClick(w, x, y, button);
    }

    override bool OnDoubleClick(Widget w, int x, int y, int button)
    {
        if (w == m_Map && button == MouseState.LEFT && !Modal() && !m_Ruler)
        {
            OpenEditor(m_Map.ScreenToMap(Vector(x, y, 0)));
            return true;
        }
        return super.OnDoubleClick(w, x, y, button);
    }

    override bool OnKeyPress(Widget w, int x, int y, int key)
    {
        if (key == KeyCode.KC_ESCAPE)
        {
            HandleBack();
            return true;
        }
        if (Modal())
            return super.OnKeyPress(w, x, y, key);
        bool control = KeyState(KeyCode.KC_LCONTROL) || KeyState(KeyCode.KC_RCONTROL);
        if (control && key == KeyCode.KC_C)
        {
            GetGame().CopyToClipboard(CoordinateText(m_CursorWorld));
            SetText("Status", "Координаты скопированы");
            return true;
        }
        if (control && key == KeyCode.KC_L)
        {
            m_Ruler = !m_Ruler;
            m_HasRulerStart = false;
            m_HasRulerEnd = false;
            SetText("Status", "Линейка: выберите две точки на карте");
            return true;
        }
        if (key == KeyCode.KC_DELETE)
        {
            GMapMarker selected = FindMarker(m_SelectedId);
            if (Editable(selected))
                OpenEditor(selected.Position, selected);
            return true;
        }
        return super.OnKeyPress(w, x, y, key);
    }

    override bool OnMouseEnter(Widget w, int x, int y)
    {
        if (ButtonWidget.Cast(w))
        {
            Widget background = w.GetParent().FindAnyWidget(w.GetName() + "Background");
            if (background)
                background.SetColor(0xFFCEE7FF);
        }
        return super.OnMouseEnter(w, x, y);
    }

    override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
    {
        if (ButtonWidget.Cast(w))
        {
            Widget background = w.GetParent().FindAnyWidget(w.GetName() + "Background");
            if (background)
                background.SetColor(0xFFFFFFFF);
        }
        return super.OnMouseLeave(w, enterW, x, y);
    }
};
