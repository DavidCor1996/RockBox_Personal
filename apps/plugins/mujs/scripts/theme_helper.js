try {
rb.clear();
rb.print("Theme audit");

function trim(text) {
    return text.replace(/^\s+|\s+$/g, "");
}

function stripRockbox(path) {
    if (!path || path === "-")
        return "";
    if (path.indexOf("/.rockbox/") === 0)
        return path.slice(10);
    if (path.indexOf(".rockbox/") === 0)
        return path.slice(9);
    return path;
}

function parseConfig(text) {
    var out = {};
    var lines = text.split("\n");
    for (var i = 0; i < lines.length; i++) {
        var line = trim(lines[i]);
        if (!line || line.charAt(0) === "#")
            continue;
        var p = line.indexOf(":");
        if (p < 0)
            continue;
        out[trim(line.slice(0, p)).toLowerCase()] = trim(line.slice(p + 1));
    }
    return out;
}

function firstExisting(paths) {
    for (var i = 0; i < paths.length; i++)
        if (rb.exists(paths[i]))
            return paths[i];
    return "";
}

function check(label, path, report, totals) {
    var clean = stripRockbox(path);
    if (!clean || clean === "-") {
        report.push(label + "=not-configured");
        return;
    }
    if (rb.exists(clean)) {
        report.push(label + "=ok " + clean);
        return;
    }
    totals.missing++;
    report.push(label + "=missing " + clean);
}

var cfgPath = firstExisting([
    "themes/iPoneCustom.cfg",
    "themes/ipone-custom-blue-shot.cfg",
    "themes/iPone.cfg",
    "themes/iPone7G.cfg",
    "themes/iPone_3g.cfg",
    "themes/iPone_nano3g.cfg",
    "themes/iPone_nano2g.cfg",
    "themes/Forest.cfg",
    "themes/SpringPod3.cfg"
]);

var report = [
    "theme-helper-report",
    "mode=ipone-designer-dependency-check",
    "generated-at-ticks=" + rb.ticks()
];
var totals = { missing: 0 };

if (!cfgPath) {
    report.push("theme=missing");
    totals.missing++;
} else {
    var cfg = parseConfig(rb.readText(cfgPath));
    report.push("theme=" + cfgPath);
    check("wps", cfg.wps, report, totals);
    check("sbs", cfg.sbs, report, totals);
    check("fms", cfg.fms, report, totals);
    check("font", cfg.font, report, totals);
    check("iconset", cfg.iconset, report, totals);
    check("backdrop", cfg.backdrop, report, totals);
}

report.push("missing-total=" + totals.missing);

rb.writeText("data/theme_helper_report.txt", report.join("\n") + "\n");
rb.print("Theme checked");
rb.print("Missing: " + totals.missing);
rb.print("Report written");
} catch (e) {
    rb.writeText("data/theme_helper_report.txt",
        "theme-helper-report\nmode=ipone-designer-dependency-check\nerror=" + e + "\n");
    rb.print("Audit failed");
}
