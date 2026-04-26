# Workaround: STM32CubeIDE 1.19.0 ships mismatched OpenOCD binary (v2.4) and scripts (v2.3), causing infinite recursion in swj_newdap <-> hla newtap

if [catch {transport select}] {
  echo "Error: unable to select a session transport. Can't continue."
  shutdown
}

proc swj_newdap {chip tag args} {
 if [using_jtag] {
     eval jtag newtap $chip $tag $args
 } elseif [using_swd] {
     eval swd newdap $chip $tag $args
 }
}
