\name Segments
\def black  #000000
\def red    #ff0000
\def medred #aa0000
\def dkred  #660000

  header: bg => black, fg => red, line => red -1, accent => medred
	  shadow => medred, shine => dkred,
	  gradient.top => black,
	  gradient.middle => black,
	  gradient.bottom => black,
	  gradient.bar => black
   music: bar => dkred
 battery: border => red, bg => black, fill.normal => red +1, fill.low => dkred +1, fill.charge => red +1,
	  bg.low => red, bg.charging =>black, chargingbolt => red

    lock: border => red, fill => red
 loadavg: bg => black, fg => dkred, spike => red

  window: bg => black, fg => red, border => medred -3
  dialog: bg => black, fg => red, line => red,
          title.fg => red,	
          button.bg => black, button.fg => red, button.border => medred,
	  button.sel.bg => medred, button.sel.fg => red, button.sel.border => red, button.sel.inner => red +1
   error: bg => medred, fg => red, line => red,
          title.fg => red,
          button.bg => medred, button.fg => red, button.border => red,
          button.sel.bg => dkred, button.sel.fg => black, button.sel.border => red, button.sel.inner => medred +1
  scroll: box => red, bg => black +1, bar => dkred +2
   input: bg => black, fg => red, selbg => medred, selfg => dkred, border => medred, cursor => dkred

    menu: bg => black, fg => dkred, choice => red, icon => red,
          selbg => black, selfg => red, selchoice => black,
          icon0 => black, icon1 => dkred, icon2 => medred, icon3 => red
  slider: border => red, bg => black, full => red
textarea: bg => black, fg => red

# calendar uses "default" for most days, "selected" for selected (duh)
#		"special" is 'today'
box:
	default.bg => black,
	default.fg => red,
	default.border => medred,
	selected.bg => dkred,
	selected.fg => black,
	selected.border => medred,
	special.bg => medred,
	special.fg => red,
	special.border => dkred

button:
	default.bg => medred,
	default.fg => red,
	default.border => red,
	selected.bg => dkred,
	selected.fg => black,
	selected.border => red
