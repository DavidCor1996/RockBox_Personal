\name Yankees
\def black  #000000
\def white  #ffffff
\def blue   #000099
\def dkblue #000066
\def grey   #999999
\def dkgrey #666666

  header: bg => @yankees-(header).png, fg => white, line => dkgrey -1, accent => grey
	  shadow => dkgrey, shine => grey,
	  gradient.top => blue,
	  gradient.middle => dkblue,
	  gradient.bottom => dkgrey,
	  gradient.bar => grey
   music: bar => dkgreyblue
 battery: border => blue, bg => dkgrey, fill.normal => blue +1, fill.low => dkgrey +1, fill.charge => blue +1,
	  bg.low => dkgrey, bg.charging => grey, chargingbolt => blue
    lock: border => dkgrey, fill => dkgrey
 loadavg: bg => grey, fg => dkgrey, spike => blue

  window: bg => blue, fg => dkgrey, border => grey -3
  dialog: bg => blue, fg => dkgrey, line => white,
          title.fg => white,	
          button.bg => blue, button.fg => dkgrey, button.border => grey,
	  button.sel.bg => grey, button.sel.fg => dkgrey, button.sel.border => dkgrey, button.sel.inner => dkgrey +1
   error: bg => grey, fg => dkgrey, line => dkgrey,
          title.fg => dkgrey,
          button.bg => grey, button.fg => dkgrey, button.border => dkgrey,
          button.sel.bg => dkgrey, button.sel.fg => blue, button.sel.border => dkgrey, button.sel.inner => grey +1
  scroll: box => dkgrey, bg => blue +1, bar => dkgrey +2
   input: bg => blue, fg => dkgrey, selbg => dkgrey, selfg => blue, border => grey, cursor => dkgrey

    menu: bg => blue, fg => dkgrey, choice => dkgrey, icon => dkgrey,
          selbg => dkgrey, selfg => blue, selchoice => blue, icon0 => dkgrey,
          icon0 => dkgrey, icon1 => dkgrey, icon2 => grey, icon3 => blue
  slider: border => dkgrey, bg => blue, full => dkgrey
textarea: bg => blue, fg => dkgrey

# calendar uses "default" for most days, "selected" for selected (duh)
#		"special" is 'today'
box:
	default.bg => blue,
	default.fg => dkgrey,
	default.border => grey,
	selected.bg => dkgrey,
	selected.fg => blue,
	selected.border => grey,
	special.bg => grey,
	special.fg => dkgrey,
	special.border => dkgrey

button:
	default.bg => grey,
	default.fg => dkgrey,
	default.border => dkgrey,
	selected.bg => dkgrey,
	selected.fg => blue,
	selected.border => dkgrey
