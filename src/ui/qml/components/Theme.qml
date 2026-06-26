pragma Singleton

import QtQuick

QtObject {
    readonly property bool dark: ThemeService.dark

    readonly property color appBackground: dark ? "#0F1117" : "#F3F6FB"
    readonly property color sidebarBackground: dark ? "#0B0E14" : "#E9EEF6"
    readonly property color canvasBackground: dark ? "#11151D" : "#F7F9FC"
    readonly property color canvasAlternate: dark ? "#121720" : "#F1F4F8"
    readonly property color surface: dark ? "#161A23" : "#FFFFFF"
    readonly property color surfaceElevated: dark ? "#161B22" : "#F8FAFD"
    readonly property color surfaceMuted: dark ? "#151A23" : "#EEF2F7"
    readonly property color surfaceHover: dark ? "#202638" : "#E4EAF3"
    readonly property color surfacePressed: dark ? "#252C3E" : "#D8E0EC"
    readonly property color surfaceSelected: dark ? "#252D44" : "#E0E7FF"
    readonly property color inputBackground: dark ? "#151A23" : "#F2F5F9"
    readonly property color controlDisabled: dark ? "#242B3A" : "#E6EAF0"

    readonly property color primaryText: dark ? "#E6EAF2" : "#172033"
    readonly property color strongText: dark ? "#FFFFFF" : "#111827"
    readonly property color secondaryText: dark ? "#9AA4B2" : "#64748B"
    readonly property color tertiaryText: dark ? "#8D98AB" : "#7C899D"
    readonly property color mutedText: dark ? "#667187" : "#94A3B8"

    readonly property color border: dark ? "#30384C" : "#D7DEE9"
    readonly property color borderSoft: dark ? "#2C3548" : "#E2E8F0"
    readonly property color divider: dark ? "#232836" : "#DCE3ED"
    readonly property color gridLine: dark ? "#202738" : "#E3E8F0"
    readonly property color gridLineSoft: dark ? "#171D29" : "#EDF1F6"

    readonly property color accent: "#6C63FF"
    readonly property color accentBright: dark ? "#7C8CFF" : "#5267E8"
    readonly property color accentSoft: dark ? "#252D44" : "#E0E7FF"
    readonly property color success: dark ? "#A9F0C9" : "#168A52"
    readonly property color warning: dark ? "#F6C56B" : "#B96A06"
    readonly property color error: dark ? "#FFB09B" : "#C83C32"
    readonly property color highPriority: "#EF4444"
    readonly property color highPriorityGlow: dark ? "#66EF4444" : "#55DC2626"
    readonly property color successSurface: dark ? "#16251F" : "#E8F7EF"
    readonly property color successBorder: dark ? "#4FAE85" : "#70B996"
    readonly property color warningSurface: dark ? "#2B2418" : "#FFF5DA"
    readonly property color warningBorder: dark ? "#80642A" : "#E1B858"
    readonly property color dangerSurface: dark ? "#2A1D1B" : "#FDEDEA"
    readonly property color dangerSurfaceHover: dark ? "#3A211E" : "#F9DDD8"
    readonly property color dangerBorder: dark ? "#FF7A59" : "#DE7565"
    readonly property color infoSurface: dark ? "#202842" : "#E8EDFF"
    readonly property color infoBorder: dark ? "#56627F" : "#9AA8D8"

    readonly property color blockCompleted: dark ? "#262B35" : "#E7EBF1"
    readonly property color blockCompletedSelected: dark ? "#3A3F4B" : "#D9E0EA"
    readonly property color blockCompletedAccent: dark ? "#8A92A3" : "#7C879A"
    readonly property color blockSelected: dark ? "#343058" : "#E7E9FF"
    readonly property color blockHandle: dark ? "#F5F7FF" : "#334155"

    readonly property color overlay: dark ? "#B00A0E17" : "#660F172A"
    readonly property color shadow: dark ? "#66000000" : "#260F172A"
    readonly property color glassSurface: dark ? "#CC121720" : "#E6FFFFFF"
    readonly property color glassBorder: dark ? "#35FFFFFF" : "#66FFFFFF"
    readonly property color glassHighlight: dark ? "#70FFFFFF" : "#B8FFFFFF"
    readonly property color white: "#FFFFFF"
    readonly property color accentText: "#FFFFFF"
}
