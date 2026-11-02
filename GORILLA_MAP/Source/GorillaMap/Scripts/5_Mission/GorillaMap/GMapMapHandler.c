// Explicit event bridge for MapWidget and interactive controls.
// Weak owner avoids a menu -> handler -> menu ownership cycle.
class GMapMapHandler : ScriptedWidgetEventHandler
{
    protected GMapMenu m_Menu;

    void GMapMapHandler(GMapMenu menu)
    {
        m_Menu = menu;
    }

    void Detach()
    {
        m_Menu = null;
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (m_Menu) return m_Menu.OnClick(w, x, y, button);
        return false;
    }

    override bool OnDoubleClick(Widget w, int x, int y, int button)
    {
        if (m_Menu) return m_Menu.OnDoubleClick(w, x, y, button);
        return false;
    }

    override bool OnKeyPress(Widget w, int x, int y, int key)
    {
        if (m_Menu) return m_Menu.OnKeyPress(w, x, y, key);
        return false;
    }

    override bool OnMouseEnter(Widget w, int x, int y)
    {
        if (m_Menu) return m_Menu.OnMouseEnter(w, x, y);
        return false;
    }

    override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
    {
        if (m_Menu) return m_Menu.OnMouseLeave(w, enterW, x, y);
        return false;
    }
    // Drag, mouse down/up and wheel deliberately remain native MapWidget events.
};
