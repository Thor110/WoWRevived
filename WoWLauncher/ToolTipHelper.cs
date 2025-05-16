public static class ToolTipHelper
{
    public static void EnableTooltips(Control.ControlCollection controls, ToolTip tooltip, params Type[] excludedTypes)
    {
        foreach (Control control in controls)
        {
            if (!excludedTypes.Contains(control.GetType()))
            {
                control.MouseEnter += (s, e) => tooltip.Show(control.AccessibleDescription ?? "No description available.", control);
                control.MouseLeave += (s, e) => tooltip.Hide(control);
            }
            if (control.HasChildren)
            {
                EnableTooltips(control.Controls, tooltip, excludedTypes); // Recursive for nested controls
            }
        }
    }
}