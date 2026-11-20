# mul (extensión M) debe detener la simulación con un error explícito.
run
expect error
expect pc 0xC
expect a2 13
expect mem32 0x100 0
