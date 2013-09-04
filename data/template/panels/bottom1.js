{
  "global": {
    "edge": "bottom",
    "align": "left",
    "edge_margin": 0,
    "align_margin": 0,
    "oriented_width_type": "percent",
    "oriented_width": 100,
    "oriented_height": 26,
    "round_corners": false,
    "round_corners_radius": 7,
    "rgba_transparency": false,
    "stretch_background": false,
    "background": false,
    "transparent": false,
    "alpha": 255,
    "tint_color": "#ffffff",
    "visibility_mode": "always",
    "height_when_hidden": 1,
    "set_strut": 1,
    "use_font_color": false,
    "font_color": "#000000",
    "use_font_size": false,
    "font_size": 10,
    "icon_size": 24,
    "padding_top": 2,
    "padding_bottom": 2,
    "padding_left": 2,
    "padding_right": 2,
    "applet_spacing": 2,
    "GtkWidgetName": "PanelToplevel"
  },
  "plugins": [
    {
      "type": "menu",
      "expand": false,
      "padding": 0,
      "border": 0
    },
    {
      "settings": {
        "buttons": [
          {
            "id": "/usr/share/applications/firefox.desktop"
          }
        ]
      },
      "type": "launchbar",
      "expand": false,
      "padding": 0,
      "border": 0
    },
    {
      "settings": {
        "tooltips": true,
        "show_icons_titles": "Both",
        "custom_fallback_icon": "xorg",
        "show_iconified": true,
        "show_mapped": true,
        "show_all_desks": false,
        "show_urgency_all_desks": true,
        "use_urgency_hint": true,
        "flat_inactive_buttons": false,
        "flat_active_button": false,
        "colorize_buttons": true,
        "use_thumbnails_as_icons": false,
        "dim_iconified": true,
        "task_width_max": 200,
        "spacing": 1,
        "highlight_modified_titles": false,
        "highlight_title_of_focused_button": false,
        "bold_font_on_mouse_over": false,
        "mode": "Classic",
        "group_fold_threshold": 5,
        "panel_fold_threshold": 10,
        "group_by": "Class",
        "manual_grouping": true,
        "unfold_focused_group": false,
        "show_single_group": false,
        "show_close_buttons": false,
        "sort_by[0]": "Timestamp",
        "sort_by[1]": "Timestamp",
        "sort_by[2]": "Timestamp",
        "sort_reverse[0]": false,
        "sort_reverse[1]": false,
        "sort_reverse[2]": false,
        "rearrange": false,
        "button1_action": "RaiseIconify",
        "button2_action": "Shade",
        "button3_action": "Menu",
        "scroll_up_action": "PrevWindow",
        "scroll_down_action": "NextWindow",
        "shift_button1_action": "Iconify",
        "shift_button2_action": "Maximize",
        "shift_button3_action": "Close",
        "shift_scroll_up_action": "PrevWindowInCurrentGroup",
        "shift_scroll_down_action": "NextWindowInCurrentGroup",
        "menu_actions_click_press": "Press",
        "other_actions_click_press": "Click",
        "mouse_over_action": "None",
        "use_group_separators": false,
        "group_separator_size": 0,
        "hide_from_launchbar": false,
        "use_x_net_wm_icon_geometry": false,
        "use_x_window_position": false
      },
      "type": "taskbar",
      "expand": true,
      "padding": 0,
      "border": 0
    },
    {
      "settings": {
        "display_in_frame": true,
        "use_custom_icon_size": true,
        "icon_size": 16
      },
      "type": "notification_area",
      "expand": false,
      "padding": 0,
      "border": 0
    },
    {
      "settings": {
        "hide_if_no_battery": false,
        "alarmCommand": "xmessage Battery low",
        "alarmTime": 5,
        "display_as": "Text",
        "background_color": "rgb(0,0,0)",
        "border_width": 3,
        "charging_color1": "rgb(27,59,198)",
        "charging_color2": "rgb(181,47,195)",
        "discharging_color1": "rgb(0,255,0)",
        "discharging_color2": "rgb(255,0,0)"
      },
      "type": "batt",
      "expand": false,
      "padding": 0,
      "border": 0
    },
    {
      "settings": {
        "clock_format": "%R",
        "tooltip_format": "%A %x",
        "icon_only": false,
        "center_text": false
      },
      "type": "dclock",
      "expand": false,
      "padding": 0,
      "border": 0
    }
  ]
}