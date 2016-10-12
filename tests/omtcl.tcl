proc doAction {msg} {
  set fd [open "rsyslog.out.log" a]
  puts $fd "message processed:"
  foreach {k v} $msg {
    puts $fd "  $k: <<$v>>"
  }
  puts $fd "  uppercase message: <<[string toupper [dict get $msg message]]>>"
  close $fd
}
