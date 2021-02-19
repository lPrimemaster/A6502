* = $1000

jsr routine1
ldx #$AB
jmp end

routine1
	lda #$FF
	rts

end