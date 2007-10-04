section .rodata
	global _start

_start:
  push 12
  push 123456789
  push dword [12]
  push dword [123456789]

  push eax
  push ecx
  push edx
  push ebx
  push esp
  push ebp
  push esi
  push edi

  push dword [eax+12]
  push dword [ecx+12]
  push dword [edx+12]
  push dword [ebx+12]
  push dword [esp+12]
  push dword [ebp+12]
  push dword [esi+12]
  push dword [edi+12]

  push dword [eax+123456789]
  push dword [ecx+123456789]
  push dword [edx+123456789]
  push dword [ebx+123456789]
  push dword [esp+123456789]
  push dword [ebp+123456789]
  push dword [esi+123456789]
  push dword [edi+123456789]

  pop dword [12]
  pop dword [123456789]

  pop dword EAX
  pop dword ECX
  pop dword EDX
  pop dword EBX
  pop dword ESP
  pop dword EBP
  pop dword ESI
  pop dword EDI

  pop dword [EAX+12]
  pop dword [ECX+12]
  pop dword [EDX+12]
  pop dword [EBX+12]
  pop dword [ESP+12]
  pop dword [EBP+12]
  pop dword [ESI+12]
  pop dword [EDI+12]

  pop dword [EAX+123456789]
  pop dword [ECX+123456789]
  pop dword [EDX+123456789]
  pop dword [EBX+123456789]
  pop dword [ESP+123456789]
  pop dword [EBP+123456789]
  pop dword [ESI+123456789]
  pop dword [EDI+123456789]

  mov dword EBX,EAX
  mov dword EBX,ECX
  mov dword EBX,EDX
  mov dword EBX,EBX
  mov dword EBX,ESP
  mov dword EBX,EBP
  mov dword EBX,ESI
  mov dword EBX,EDI

  mov dword EAX,EBX
  mov dword ECX,EBX
  mov dword EDX,EBX
  mov dword EBX,EBX
  mov dword ESP,EBX
  mov dword EBP,EBX
  mov dword ESI,EBX
  mov dword EDI,EBX

  mov dword EAX,[12]
  mov dword ECX,[12]
  mov dword EDX,[12]
  mov dword EBX,[12]
  mov dword ESP,[12]
  mov dword EBP,[12]
  mov dword ESI,[12]
  mov dword EDI,[12]

  mov dword EAX,[123456789]
  mov dword ECX,[123456789]
  mov dword EDX,[123456789]
  mov dword EBX,[123456789]
  mov dword ESP,[123456789]
  mov dword EBP,[123456789]
  mov dword ESI,[123456789]
  mov dword EDI,[123456789]

  mov dword EBX,[EAX+12]
  mov dword EBX,[ECX+12]
  mov dword EBX,[EDX+12]
  mov dword EBX,[EBX+12]
  mov dword EBX,[ESP+12]
  mov dword EBX,[EBP+12]
  mov dword EBX,[ESI+12]
  mov dword EBX,[EDI+12]

  mov dword EBX,[EAX+123456789]
  mov dword EBX,[ECX+123456789]
  mov dword EBX,[EDX+123456789]
  mov dword EBX,[EBX+123456789]
  mov dword EBX,[ESP+123456789]
  mov dword EBX,[EBP+123456789]
  mov dword EBX,[ESI+123456789]
  mov dword EBX,[EDI+123456789]

  mov dword EAX,[EBX+12]
  mov dword ECX,[EBX+12]
  mov dword EDX,[EBX+12]
  mov dword EBX,[EBX+12]
  mov dword ESP,[EBX+12]
  mov dword EBP,[EBX+12]
  mov dword ESI,[EBX+12]
  mov dword EDI,[EBX+12]

  mov dword EAX,[EBX+123456789]
  mov dword ECX,[EBX+123456789]
  mov dword EDX,[EBX+123456789]
  mov dword EBX,[EBX+123456789]
  mov dword ESP,[EBX+123456789]
  mov dword EBP,[EBX+123456789]
  mov dword ESI,[EBX+123456789]
  mov dword EDI,[EBX+123456789]

  mov dword EAX,12
  mov dword ECX,12
  mov dword EDX,12
  mov dword EBX,12
  mov dword ESP,12
  mov dword EBP,12
  mov dword ESI,12
  mov dword EDI,12

  mov dword EAX,123456789
  mov dword ECX,123456789
  mov dword EDX,123456789
  mov dword EBX,123456789
  mov dword ESP,123456789
  mov dword EBP,123456789
  mov dword ESI,123456789
  mov dword EDI,123456789

  mov dword [12],EAX
  mov dword [12],ECX
  mov dword [12],EDX
  mov dword [12],EBX
  mov dword [12],ESP
  mov dword [12],EBP
  mov dword [12],ESI
  mov dword [12],EDI

  mov dword [123456789],EAX
  mov dword [123456789],ECX
  mov dword [123456789],EDX
  mov dword [123456789],EBX
  mov dword [123456789],ESP
  mov dword [123456789],EBP
  mov dword [123456789],ESI
  mov dword [123456789],EDI

  mov dword [EBX+12],EAX
  mov dword [EBX+12],ECX
  mov dword [EBX+12],EDX
  mov dword [EBX+12],EBX
  mov dword [EBX+12],ESP
  mov dword [EBX+12],EBP
  mov dword [EBX+12],ESI
  mov dword [EBX+12],EDI

  mov dword [EBX+123456789],EAX
  mov dword [EBX+123456789],ECX
  mov dword [EBX+123456789],EDX
  mov dword [EBX+123456789],EBX
  mov dword [EBX+123456789],ESP
  mov dword [EBX+123456789],EBP
  mov dword [EBX+123456789],ESI
  mov dword [EBX+123456789],EDI

  mov dword [EAX+12],EBX
  mov dword [ECX+12],EBX
  mov dword [EDX+12],EBX
  mov dword [EBX+12],EBX
  mov dword [ESP+12],EBX
  mov dword [EBP+12],EBX
  mov dword [ESI+12],EBX
  mov dword [EDI+12],EBX

  mov dword [EAX+12],99
  mov dword [ECX+12],99
  mov dword [EDX+12],99
  mov dword [EBX+12],99
  mov dword [ESP+12],99
  mov dword [EBP+12],99
  mov dword [ESI+12],99
  mov dword [EDI+12],99

  mov dword [EAX+123456789],99
  mov dword [ECX+123456789],99
  mov dword [EDX+123456789],99
  mov dword [EBX+123456789],99
  mov dword [ESP+123456789],99
  mov dword [EBP+123456789],99
  mov dword [ESI+123456789],99
  mov dword [EDI+123456789],99

  mov dword EBX,[EAX+0]
  mov dword EBX,[ECX+0]
  mov dword EBX,[EDX+0]
  mov dword EBX,[EBX+0]
  mov dword EBX,[ESP+0]
  mov dword EBX,[EBP+0]
  mov dword EBX,[ESI+0]
  mov dword EBX,[EDI+0]

  cmp dword EBX,EAX
  cmp dword EBX,ECX
  cmp dword EBX,EDX
  cmp dword EBX,EBX
  cmp dword EBX,ESP
  cmp dword EBX,EBP
  cmp dword EBX,ESI
  cmp dword EBX,EDI

  cmp dword EAX,EBX
  cmp dword ECX,EBX
  cmp dword EDX,EBX
  cmp dword EBX,EBX
  cmp dword ESP,EBX
  cmp dword EBP,EBX
  cmp dword ESI,EBX
  cmp dword EDI,EBX

  cmp dword EAX,99
  cmp dword ECX,99
  cmp dword EDX,99
  cmp dword EBX,99
  cmp dword ESP,99
  cmp dword EBP,99
  cmp dword ESI,99
  cmp dword EDI,99

  cmp dword [12],99
  cmp dword [123456789],99

  cmp dword [EAX+12],99
  cmp dword [ECX+12],99
  cmp dword [EDX+12],99
  cmp dword [EBX+12],99
  cmp dword [ESP+12],99
  cmp dword [EBP+12],99
  cmp dword [ESI+12],99
  cmp dword [EDI+12],99

  cmp dword [EAX+123456789],99
  cmp dword [ECX+123456789],99
  cmp dword [EDX+123456789],99
  cmp dword [EBX+123456789],99
  cmp dword [ESP+123456789],99
  cmp dword [EBP+123456789],99
  cmp dword [ESI+123456789],99
  cmp dword [EDI+123456789],99

  cmp dword EBX,[EAX]
  cmp dword EBX,[ECX]
  cmp dword EBX,[EDX]
  cmp dword EBX,[EBX]
  cmp dword EBX,[ESP]
  cmp dword EBX,[EBP]
  cmp dword EBX,[ESI]
  cmp dword EBX,[EDI]

  cmp dword EAX,[EBX]
  cmp dword ECX,[EBX]
  cmp dword EDX,[EBX]
  cmp dword EBX,[EBX]
  cmp dword ESP,[EBX]
  cmp dword EBP,[EBX]
  cmp dword ESI,[EBX]
  cmp dword EDI,[EBX]

  cmp dword EBX,[EAX+12]
  cmp dword EBX,[ECX+12]
  cmp dword EBX,[EDX+12]
  cmp dword EBX,[EBX+12]
  cmp dword EBX,[ESP+12]
  cmp dword EBX,[EBP+12]
  cmp dword EBX,[ESI+12]
  cmp dword EBX,[EDI+12]

  cmp dword EAX,[EBX+12]
  cmp dword ECX,[EBX+12]
  cmp dword EDX,[EBX+12]
  cmp dword EBX,[EBX+12]
  cmp dword ESP,[EBX+12]
  cmp dword EBP,[EBX+12]
  cmp dword ESI,[EBX+12]
  cmp dword EDI,[EBX+12]

  cmp dword EBX,[EAX+123456789]
  cmp dword EBX,[ECX+123456789]
  cmp dword EBX,[EDX+123456789]
  cmp dword EBX,[EBX+123456789]
  cmp dword EBX,[ESP+123456789]
  cmp dword EBX,[EBP+123456789]
  cmp dword EBX,[ESI+123456789]
  cmp dword EBX,[EDI+123456789]

  cmp dword EAX,[EBX+123456789]
  cmp dword ECX,[EBX+123456789]
  cmp dword EDX,[EBX+123456789]
  cmp dword EBX,[EBX+123456789]
  cmp dword ESP,[EBX+123456789]
  cmp dword EBP,[EBX+123456789]
  cmp dword ESI,[EBX+123456789]
  cmp dword EDI,[EBX+123456789]

  cmp dword EAX,[12]
  cmp dword ECX,[12]
  cmp dword EDX,[12]
  cmp dword EBX,[12]
  cmp dword ESP,[12]
  cmp dword EBP,[12]
  cmp dword ESI,[12]
  cmp dword EDI,[12]

  cmp dword EAX,[123456789]
  cmp dword ECX,[123456789]
  cmp dword EDX,[123456789]
  cmp dword EBX,[123456789]
  cmp dword ESP,[123456789]
  cmp dword EBP,[123456789]
  cmp dword ESI,[123456789]
  cmp dword EDI,[123456789]

  jmp EAX
  jmp ECX
  jmp EDX
  jmp EBX
  jmp ESP
  jmp EBP
  jmp ESI
  jmp EDI

  jmp [EAX+0]
  jmp [ECX+0]
  jmp [EDX+0]
  jmp [EBX+0]
  jmp [ESP+0]
  jmp [EBP+0]
  jmp [ESI+0]
  jmp [EDI+0]

  jmp [EAX+12]
  jmp [ECX+12]
  jmp [EDX+12]
  jmp [EBX+12]
  jmp [ESP+12]
  jmp [EBP+12]
  jmp [ESI+12]
  jmp [EDI+12]

  jmp [EAX+123456789]
  jmp [ECX+123456789]
  jmp [EDX+123456789]
  jmp [EBX+123456789]
  jmp [ESP+123456789]
  jmp [EBP+123456789]
  jmp [ESI+123456789]
  jmp [EDI+123456789]

  jmp [12]
  jmp [123456789]

  ret

  call EAX
  call ECX
  call EDX
  call EBX
  call ESP
  call EBP
  call ESI
  call EDI

  call [EAX+0]
  call [ECX+0]
  call [EDX+0]
  call [EBX+0]
  call [ESP+0]
  call [EBP+0]
  call [ESI+0]
  call [EDI+0]

  call [EAX+12]
  call [ECX+12]
  call [EDX+12]
  call [EBX+12]
  call [ESP+12]
  call [EBP+12]
  call [ESI+12]
  call [EDI+12]

  call [EAX+123456789]
  call [ECX+123456789]
  call [EDX+123456789]
  call [EBX+123456789]
  call [ESP+123456789]
  call [EBP+123456789]
  call [ESI+123456789]
  call [EDI+123456789]

  call [12]
  call [123456789]

  add dword EAX,12
  add dword ECX,12
  add dword EDX,12
  add dword EBX,12
  add dword ESP,12
  add dword EBP,12
  add dword ESI,12
  add dword EDI,12

  add dword EBX,EAX
  add dword EBX,ECX
  add dword EBX,EDX
  add dword EBX,EBX
  add dword EBX,ESP
  add dword EBX,EBP
  add dword EBX,ESI
  add dword EBX,EDI

  add dword EAX,EBX
  add dword ECX,EBX
  add dword EDX,EBX
  add dword EBX,EBX
  add dword ESP,EBX
  add dword EBP,EBX
  add dword ESI,EBX
  add dword EDI,EBX

  add dword EBX,[EAX]
  add dword EBX,[ECX]
  add dword EBX,[EDX]
  add dword EBX,[EBX]
  add dword EBX,[ESP]
  add dword EBX,[EBP]
  add dword EBX,[ESI]
  add dword EBX,[EDI]

  add dword EBX,[EAX+12]
  add dword EBX,[ECX+12]
  add dword EBX,[EDX+12]
  add dword EBX,[EBX+12]
  add dword EBX,[ESP+12]
  add dword EBX,[EBP+12]
  add dword EBX,[ESI+12]
  add dword EBX,[EDI+12]

  add dword EBX,[EAX+123456789]
  add dword EBX,[ECX+123456789]
  add dword EBX,[EDX+123456789]
  add dword EBX,[EBX+123456789]
  add dword EBX,[ESP+123456789]
  add dword EBX,[EBP+123456789]
  add dword EBX,[ESI+123456789]
  add dword EBX,[EDI+123456789]

  add dword EAX,[EBX]
  add dword ECX,[EBX]
  add dword EDX,[EBX]
  add dword EBX,[EBX]
  add dword ESP,[EBX]
  add dword EBP,[EBX]
  add dword ESI,[EBX]
  add dword EDI,[EBX]

  add dword EAX,[EBX+12]
  add dword ECX,[EBX+12]
  add dword EDX,[EBX+12]
  add dword EBX,[EBX+12]
  add dword ESP,[EBX+12]
  add dword EBP,[EBX+12]
  add dword ESI,[EBX+12]
  add dword EDI,[EBX+12]

  add dword EAX,[EBX+123456789]
  add dword ECX,[EBX+123456789]
  add dword EDX,[EBX+123456789]
  add dword EBX,[EBX+123456789]
  add dword ESP,[EBX+123456789]
  add dword EBP,[EBX+123456789]
  add dword ESI,[EBX+123456789]
  add dword EDI,[EBX+123456789]

  add dword EAX,[12]
  add dword ECX,[12]
  add dword EDX,[12]
  add dword EBX,[12]
  add dword ESP,[12]
  add dword EBP,[12]
  add dword ESI,[12]
  add dword EDI,[12]

  add dword EAX,[123456789]
  add dword ECX,[123456789]
  add dword EDX,[123456789]
  add dword EBX,[123456789]
  add dword ESP,[123456789]
  add dword EBP,[123456789]
  add dword ESI,[123456789]
  add dword EDI,[123456789]

  add dword [EAX],99
  add dword [ECX],99
  add dword [EDX],99
  add dword [EBX],99
  add dword [ESP],99
  add dword [EBP],99
  add dword [ESI],99
  add dword [EDI],99

  add dword [EAX+12],99
  add dword [ECX+12],99
  add dword [EDX+12],99
  add dword [EBX+12],99
  add dword [ESP+12],99
  add dword [EBP+12],99
  add dword [ESI+12],99
  add dword [EDI+12],99

  add dword [EAX+123456789],99
  add dword [ECX+123456789],99
  add dword [EDX+123456789],99
  add dword [EBX+123456789],99
  add dword [ESP+123456789],99
  add dword [EBP+123456789],99
  add dword [ESI+123456789],99
  add dword [EDI+123456789],99

  add dword [12],99
  add dword [123456789],99

  sub dword EAX,12
  sub dword ECX,12
  sub dword EDX,12
  sub dword EBX,12
  sub dword ESP,12
  sub dword EBP,12
  sub dword ESI,12
  sub dword EDI,12

  sub dword EBX,EAX
  sub dword EBX,ECX
  sub dword EBX,EDX
  sub dword EBX,EBX
  sub dword EBX,ESP
  sub dword EBX,EBP
  sub dword EBX,ESI
  sub dword EBX,EDI

  sub dword EAX,EBX
  sub dword ECX,EBX
  sub dword EDX,EBX
  sub dword EBX,EBX
  sub dword ESP,EBX
  sub dword EBP,EBX
  sub dword ESI,EBX
  sub dword EDI,EBX

  sub dword EBX,[EAX]
  sub dword EBX,[ECX]
  sub dword EBX,[EDX]
  sub dword EBX,[EBX]
  sub dword EBX,[ESP]
  sub dword EBX,[EBP]
  sub dword EBX,[ESI]
  sub dword EBX,[EDI]

  sub dword EBX,[EAX+12]
  sub dword EBX,[ECX+12]
  sub dword EBX,[EDX+12]
  sub dword EBX,[EBX+12]
  sub dword EBX,[ESP+12]
  sub dword EBX,[EBP+12]
  sub dword EBX,[ESI+12]
  sub dword EBX,[EDI+12]

  sub dword EBX,[EAX+123456789]
  sub dword EBX,[ECX+123456789]
  sub dword EBX,[EDX+123456789]
  sub dword EBX,[EBX+123456789]
  sub dword EBX,[ESP+123456789]
  sub dword EBX,[EBP+123456789]
  sub dword EBX,[ESI+123456789]
  sub dword EBX,[EDI+123456789]

  sub dword EAX,[EBX]
  sub dword ECX,[EBX]
  sub dword EDX,[EBX]
  sub dword EBX,[EBX]
  sub dword ESP,[EBX]
  sub dword EBP,[EBX]
  sub dword ESI,[EBX]
  sub dword EDI,[EBX]

  sub dword EAX,[EBX+12]
  sub dword ECX,[EBX+12]
  sub dword EDX,[EBX+12]
  sub dword EBX,[EBX+12]
  sub dword ESP,[EBX+12]
  sub dword EBP,[EBX+12]
  sub dword ESI,[EBX+12]
  sub dword EDI,[EBX+12]

  sub dword EAX,[EBX+123456789]
  sub dword ECX,[EBX+123456789]
  sub dword EDX,[EBX+123456789]
  sub dword EBX,[EBX+123456789]
  sub dword ESP,[EBX+123456789]
  sub dword EBP,[EBX+123456789]
  sub dword ESI,[EBX+123456789]
  sub dword EDI,[EBX+123456789]

  sub dword EAX,[12]
  sub dword ECX,[12]
  sub dword EDX,[12]
  sub dword EBX,[12]
  sub dword ESP,[12]
  sub dword EBP,[12]
  sub dword ESI,[12]
  sub dword EDI,[12]

  sub dword EAX,[123456789]
  sub dword ECX,[123456789]
  sub dword EDX,[123456789]
  sub dword EBX,[123456789]
  sub dword ESP,[123456789]
  sub dword EBP,[123456789]
  sub dword ESI,[123456789]
  sub dword EDI,[123456789]

  sub dword [EAX],99
  sub dword [ECX],99
  sub dword [EDX],99
  sub dword [EBX],99
  sub dword [ESP],99
  sub dword [EBP],99
  sub dword [ESI],99
  sub dword [EDI],99

  sub dword [EAX+12],99
  sub dword [ECX+12],99
  sub dword [EDX+12],99
  sub dword [EBX+12],99
  sub dword [ESP+12],99
  sub dword [EBP+12],99
  sub dword [ESI+12],99
  sub dword [EDI+12],99

  sub dword [EAX+123456789],99
  sub dword [ECX+123456789],99
  sub dword [EDX+123456789],99
  sub dword [EBX+123456789],99
  sub dword [ESP+123456789],99
  sub dword [EBP+123456789],99
  sub dword [ESI+123456789],99
  sub dword [EDI+123456789],99

  sub dword [12],99
  sub dword [123456789],99

  and dword EAX,12
  and dword ECX,12
  and dword EDX,12
  and dword EBX,12
  and dword ESP,12
  and dword EBP,12
  and dword ESI,12
  and dword EDI,12

  and dword EBX,EAX
  and dword EBX,ECX
  and dword EBX,EDX
  and dword EBX,EBX
  and dword EBX,ESP
  and dword EBX,EBP
  and dword EBX,ESI
  and dword EBX,EDI

  and dword EAX,EBX
  and dword ECX,EBX
  and dword EDX,EBX
  and dword EBX,EBX
  and dword ESP,EBX
  and dword EBP,EBX
  and dword ESI,EBX
  and dword EDI,EBX

  and dword EBX,[EAX]
  and dword EBX,[ECX]
  and dword EBX,[EDX]
  and dword EBX,[EBX]
  and dword EBX,[ESP]
  and dword EBX,[EBP]
  and dword EBX,[ESI]
  and dword EBX,[EDI]

  and dword EBX,[EAX+12]
  and dword EBX,[ECX+12]
  and dword EBX,[EDX+12]
  and dword EBX,[EBX+12]
  and dword EBX,[ESP+12]
  and dword EBX,[EBP+12]
  and dword EBX,[ESI+12]
  and dword EBX,[EDI+12]

  and dword EBX,[EAX+123456789]
  and dword EBX,[ECX+123456789]
  and dword EBX,[EDX+123456789]
  and dword EBX,[EBX+123456789]
  and dword EBX,[ESP+123456789]
  and dword EBX,[EBP+123456789]
  and dword EBX,[ESI+123456789]
  and dword EBX,[EDI+123456789]

  and dword EAX,[EBX]
  and dword ECX,[EBX]
  and dword EDX,[EBX]
  and dword EBX,[EBX]
  and dword ESP,[EBX]
  and dword EBP,[EBX]
  and dword ESI,[EBX]
  and dword EDI,[EBX]

  and dword EAX,[EBX+12]
  and dword ECX,[EBX+12]
  and dword EDX,[EBX+12]
  and dword EBX,[EBX+12]
  and dword ESP,[EBX+12]
  and dword EBP,[EBX+12]
  and dword ESI,[EBX+12]
  and dword EDI,[EBX+12]

  and dword EAX,[EBX+123456789]
  and dword ECX,[EBX+123456789]
  and dword EDX,[EBX+123456789]
  and dword EBX,[EBX+123456789]
  and dword ESP,[EBX+123456789]
  and dword EBP,[EBX+123456789]
  and dword ESI,[EBX+123456789]
  and dword EDI,[EBX+123456789]

  and dword EAX,[12]
  and dword ECX,[12]
  and dword EDX,[12]
  and dword EBX,[12]
  and dword ESP,[12]
  and dword EBP,[12]
  and dword ESI,[12]
  and dword EDI,[12]

  and dword EAX,[123456789]
  and dword ECX,[123456789]
  and dword EDX,[123456789]
  and dword EBX,[123456789]
  and dword ESP,[123456789]
  and dword EBP,[123456789]
  and dword ESI,[123456789]
  and dword EDI,[123456789]

  and dword [EAX],99
  and dword [ECX],99
  and dword [EDX],99
  and dword [EBX],99
  and dword [ESP],99
  and dword [EBP],99
  and dword [ESI],99
  and dword [EDI],99

  and dword [EAX+12],99
  and dword [ECX+12],99
  and dword [EDX+12],99
  and dword [EBX+12],99
  and dword [ESP+12],99
  and dword [EBP+12],99
  and dword [ESI+12],99
  and dword [EDI+12],99

  and dword [EAX+123456789],99
  and dword [ECX+123456789],99
  and dword [EDX+123456789],99
  and dword [EBX+123456789],99
  and dword [ESP+123456789],99
  and dword [EBP+123456789],99
  and dword [ESI+123456789],99
  and dword [EDI+123456789],99

  and dword [12],99
  and dword [123456789],99

  or dword EAX,12
  or dword ECX,12
  or dword EDX,12
  or dword EBX,12
  or dword ESP,12
  or dword EBP,12
  or dword ESI,12
  or dword EDI,12

  or dword EBX,EAX
  or dword EBX,ECX
  or dword EBX,EDX
  or dword EBX,EBX
  or dword EBX,ESP
  or dword EBX,EBP
  or dword EBX,ESI
  or dword EBX,EDI

  or dword EAX,EBX
  or dword ECX,EBX
  or dword EDX,EBX
  or dword EBX,EBX
  or dword ESP,EBX
  or dword EBP,EBX
  or dword ESI,EBX
  or dword EDI,EBX

  or dword EBX,[EAX]
  or dword EBX,[ECX]
  or dword EBX,[EDX]
  or dword EBX,[EBX]
  or dword EBX,[ESP]
  or dword EBX,[EBP]
  or dword EBX,[ESI]
  or dword EBX,[EDI]

  or dword EBX,[EAX+12]
  or dword EBX,[ECX+12]
  or dword EBX,[EDX+12]
  or dword EBX,[EBX+12]
  or dword EBX,[ESP+12]
  or dword EBX,[EBP+12]
  or dword EBX,[ESI+12]
  or dword EBX,[EDI+12]

  or dword EBX,[EAX+123456789]
  or dword EBX,[ECX+123456789]
  or dword EBX,[EDX+123456789]
  or dword EBX,[EBX+123456789]
  or dword EBX,[ESP+123456789]
  or dword EBX,[EBP+123456789]
  or dword EBX,[ESI+123456789]
  or dword EBX,[EDI+123456789]

  or dword EAX,[EBX]
  or dword ECX,[EBX]
  or dword EDX,[EBX]
  or dword EBX,[EBX]
  or dword ESP,[EBX]
  or dword EBP,[EBX]
  or dword ESI,[EBX]
  or dword EDI,[EBX]

  or dword EAX,[EBX+12]
  or dword ECX,[EBX+12]
  or dword EDX,[EBX+12]
  or dword EBX,[EBX+12]
  or dword ESP,[EBX+12]
  or dword EBP,[EBX+12]
  or dword ESI,[EBX+12]
  or dword EDI,[EBX+12]

  or dword EAX,[EBX+123456789]
  or dword ECX,[EBX+123456789]
  or dword EDX,[EBX+123456789]
  or dword EBX,[EBX+123456789]
  or dword ESP,[EBX+123456789]
  or dword EBP,[EBX+123456789]
  or dword ESI,[EBX+123456789]
  or dword EDI,[EBX+123456789]

  or dword EAX,[12]
  or dword ECX,[12]
  or dword EDX,[12]
  or dword EBX,[12]
  or dword ESP,[12]
  or dword EBP,[12]
  or dword ESI,[12]
  or dword EDI,[12]

  or dword EAX,[123456789]
  or dword ECX,[123456789]
  or dword EDX,[123456789]
  or dword EBX,[123456789]
  or dword ESP,[123456789]
  or dword EBP,[123456789]
  or dword ESI,[123456789]
  or dword EDI,[123456789]

  or dword [EAX],99
  or dword [ECX],99
  or dword [EDX],99
  or dword [EBX],99
  or dword [ESP],99
  or dword [EBP],99
  or dword [ESI],99
  or dword [EDI],99

  or dword [EAX+12],99
  or dword [ECX+12],99
  or dword [EDX+12],99
  or dword [EBX+12],99
  or dword [ESP+12],99
  or dword [EBP+12],99
  or dword [ESI+12],99
  or dword [EDI+12],99

  or dword [EAX+123456789],99
  or dword [ECX+123456789],99
  or dword [EDX+123456789],99
  or dword [EBX+123456789],99
  or dword [ESP+123456789],99
  or dword [EBP+123456789],99
  or dword [ESI+123456789],99
  or dword [EDI+123456789],99

  or dword [12],99
  or dword [123456789],99

  xor dword EAX,12
  xor dword ECX,12
  xor dword EDX,12
  xor dword EBX,12
  xor dword ESP,12
  xor dword EBP,12
  xor dword ESI,12
  xor dword EDI,12

  xor dword EBX,EAX
  xor dword EBX,ECX
  xor dword EBX,EDX
  xor dword EBX,EBX
  xor dword EBX,ESP
  xor dword EBX,EBP
  xor dword EBX,ESI
  xor dword EBX,EDI

  xor dword EAX,EBX
  xor dword ECX,EBX
  xor dword EDX,EBX
  xor dword EBX,EBX
  xor dword ESP,EBX
  xor dword EBP,EBX
  xor dword ESI,EBX
  xor dword EDI,EBX

  xor dword EBX,[EAX]
  xor dword EBX,[ECX]
  xor dword EBX,[EDX]
  xor dword EBX,[EBX]
  xor dword EBX,[ESP]
  xor dword EBX,[EBP]
  xor dword EBX,[ESI]
  xor dword EBX,[EDI]

  xor dword EBX,[EAX+12]
  xor dword EBX,[ECX+12]
  xor dword EBX,[EDX+12]
  xor dword EBX,[EBX+12]
  xor dword EBX,[ESP+12]
  xor dword EBX,[EBP+12]
  xor dword EBX,[ESI+12]
  xor dword EBX,[EDI+12]

  xor dword EBX,[EAX+123456789]
  xor dword EBX,[ECX+123456789]
  xor dword EBX,[EDX+123456789]
  xor dword EBX,[EBX+123456789]
  xor dword EBX,[ESP+123456789]
  xor dword EBX,[EBP+123456789]
  xor dword EBX,[ESI+123456789]
  xor dword EBX,[EDI+123456789]

  xor dword EAX,[EBX]
  xor dword ECX,[EBX]
  xor dword EDX,[EBX]
  xor dword EBX,[EBX]
  xor dword ESP,[EBX]
  xor dword EBP,[EBX]
  xor dword ESI,[EBX]
  xor dword EDI,[EBX]

  xor dword EAX,[EBX+12]
  xor dword ECX,[EBX+12]
  xor dword EDX,[EBX+12]
  xor dword EBX,[EBX+12]
  xor dword ESP,[EBX+12]
  xor dword EBP,[EBX+12]
  xor dword ESI,[EBX+12]
  xor dword EDI,[EBX+12]

  xor dword EAX,[EBX+123456789]
  xor dword ECX,[EBX+123456789]
  xor dword EDX,[EBX+123456789]
  xor dword EBX,[EBX+123456789]
  xor dword ESP,[EBX+123456789]
  xor dword EBP,[EBX+123456789]
  xor dword ESI,[EBX+123456789]
  xor dword EDI,[EBX+123456789]

  xor dword EAX,[12]
  xor dword ECX,[12]
  xor dword EDX,[12]
  xor dword EBX,[12]
  xor dword ESP,[12]
  xor dword EBP,[12]
  xor dword ESI,[12]
  xor dword EDI,[12]

  xor dword EAX,[123456789]
  xor dword ECX,[123456789]
  xor dword EDX,[123456789]
  xor dword EBX,[123456789]
  xor dword ESP,[123456789]
  xor dword EBP,[123456789]
  xor dword ESI,[123456789]
  xor dword EDI,[123456789]

  xor dword [EAX],99
  xor dword [ECX],99
  xor dword [EDX],99
  xor dword [EBX],99
  xor dword [ESP],99
  xor dword [EBP],99
  xor dword [ESI],99
  xor dword [EDI],99

  xor dword [EAX+12],99
  xor dword [ECX+12],99
  xor dword [EDX+12],99
  xor dword [EBX+12],99
  xor dword [ESP+12],99
  xor dword [EBP+12],99
  xor dword [ESI+12],99
  xor dword [EDI+12],99

  xor dword [EAX+123456789],99
  xor dword [ECX+123456789],99
  xor dword [EDX+123456789],99
  xor dword [EBX+123456789],99
  xor dword [ESP+123456789],99
  xor dword [EBP+123456789],99
  xor dword [ESI+123456789],99
  xor dword [EDI+123456789],99

  xor dword [12],99
  xor dword [123456789],99

  not dword EAX
  not dword ECX
  not dword EDX
  not dword EBX
  not dword ESP
  not dword EBP
  not dword ESI
  not dword EDI

  not dword [EAX]
  not dword [ECX]
  not dword [EDX]
  not dword [EBX]
  not dword [ESP]
  not dword [EBP]
  not dword [ESI]
  not dword [EDI]

  not dword [EAX+12]
  not dword [ECX+12]
  not dword [EDX+12]
  not dword [EBX+12]
  not dword [ESP+12]
  not dword [EBP+12]
  not dword [ESI+12]
  not dword [EDI+12]

  not dword [EAX+123456789]
  not dword [ECX+123456789]
  not dword [EDX+123456789]
  not dword [EBX+123456789]
  not dword [ESP+123456789]
  not dword [EBP+123456789]
  not dword [ESI+123456789]
  not dword [EDI+123456789]

  not dword [12]
  not dword [123456789]

;  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_1,EAX));
;  mov ecx,[ebx+12+1*eax]


  mov dword ECX,[EBX+12+EAX]
  mov dword ECX,[EBX+12+ECX]
  mov dword ECX,[EBX+12+EDX]
  mov dword ECX,[EBX+12+EBX]
  mov dword ECX,[EBX+12+EBP]
  mov dword ECX,[EBX+12+ESI]
  mov dword ECX,[EBX+12+EDI]

  mov dword ECX,[EAX+12+EBX]
  mov dword ECX,[ECX+12+EBX]
  mov dword ECX,[EDX+12+EBX]
  mov dword ECX,[EBX+12+EBX]
  mov dword ECX,[ESP+12+EBX]
  mov dword ECX,[EBP+12+EBX]
  mov dword ECX,[ESI+12+EBX]
  mov dword ECX,[EDI+12+EBX]

  mov dword ECX,[EBX+12+2*EAX]
  mov dword ECX,[EBX+12+2*ECX]
  mov dword ECX,[EBX+12+2*EDX]
  mov dword ECX,[EBX+12+2*EBX]
  mov dword ECX,[EBX+12+2*EBP]
  mov dword ECX,[EBX+12+2*ESI]
  mov dword ECX,[EBX+12+2*EDI]

  mov dword ECX,[EAX+12+2*EBX]
  mov dword ECX,[ECX+12+2*EBX]
  mov dword ECX,[EDX+12+2*EBX]
  mov dword ECX,[EBX+12+2*EBX]
  mov dword ECX,[ESP+12+2*EBX]
  mov dword ECX,[EBP+12+2*EBX]
  mov dword ECX,[ESI+12+2*EBX]
  mov dword ECX,[EDI+12+2*EBX]

  mov dword ECX,[EBX+12+4*EAX]
  mov dword ECX,[EBX+12+4*ECX]
  mov dword ECX,[EBX+12+4*EDX]
  mov dword ECX,[EBX+12+4*EBX]
  mov dword ECX,[EBX+12+4*EBP]
  mov dword ECX,[EBX+12+4*ESI]
  mov dword ECX,[EBX+12+4*EDI]

  mov dword ECX,[EAX+12+4*EBX]
  mov dword ECX,[ECX+12+4*EBX]
  mov dword ECX,[EDX+12+4*EBX]
  mov dword ECX,[EBX+12+4*EBX]
  mov dword ECX,[ESP+12+4*EBX]
  mov dword ECX,[EBP+12+4*EBX]
  mov dword ECX,[ESI+12+4*EBX]
  mov dword ECX,[EDI+12+4*EBX]

  mov dword ECX,[EBX+12+8*EAX]
  mov dword ECX,[EBX+12+8*ECX]
  mov dword ECX,[EBX+12+8*EDX]
  mov dword ECX,[EBX+12+8*EBX]
  mov dword ECX,[EBX+12+8*EBP]
  mov dword ECX,[EBX+12+8*ESI]
  mov dword ECX,[EBX+12+8*EDI]

  mov dword ECX,[EAX+12+8*EBX]
  mov dword ECX,[ECX+12+8*EBX]
  mov dword ECX,[EDX+12+8*EBX]
  mov dword ECX,[EBX+12+8*EBX]
  mov dword ECX,[ESP+12+8*EBX]
  mov dword ECX,[EBP+12+8*EBX]
  mov dword ECX,[ESI+12+8*EBX]
  mov dword ECX,[EDI+12+8*EBX]

  mov dword ECX,[EAX+123456789+8*EBX]
  mov dword ECX,[ECX+123456789+8*EBX]
  mov dword ECX,[EDX+123456789+8*EBX]
  mov dword ECX,[EBX+123456789+8*EBX]
  mov dword ECX,[ESP+123456789+8*EBX]
  mov dword ECX,[EBP+123456789+8*EBX]
  mov dword ECX,[ESI+123456789+8*EBX]
  mov dword ECX,[EDI+123456789+8*EBX]

  imul dword EAX,12
  imul dword ECX,12
  imul dword EDX,12
  imul dword EBX,12
  imul dword ESP,12
  imul dword EBP,12
  imul dword ESI,12
  imul dword EDI,12

  imul dword EAX,123456789
  imul dword ECX,123456789
  imul dword EDX,123456789
  imul dword EBX,123456789
  imul dword ESP,123456789
  imul dword EBP,123456789
  imul dword ESI,123456789
  imul dword EDI,123456789

  imul dword EBX,EAX
  imul dword EBX,ECX
  imul dword EBX,EDX
  imul dword EBX,EBX
  imul dword EBX,ESP
  imul dword EBX,EBP
  imul dword EBX,ESI
  imul dword EBX,EDI

  imul dword EAX,EBX
  imul dword ECX,EBX
  imul dword EDX,EBX
  imul dword EBX,EBX
  imul dword ESP,EBX
  imul dword EBP,EBX
  imul dword ESI,EBX
  imul dword EDI,EBX

  imul dword EBX,[EAX+12]
  imul dword EBX,[ECX+12]
  imul dword EBX,[EDX+12]
  imul dword EBX,[EBX+12]
  imul dword EBX,[ESP+12]
  imul dword EBX,[EBP+12]
  imul dword EBX,[ESI+12]
  imul dword EBX,[EDI+12]

  imul dword EAX,[EBX+12]
  imul dword ECX,[EBX+12]
  imul dword EDX,[EBX+12]
  imul dword EBX,[EBX+12]
  imul dword ESP,[EBX+12]
  imul dword EBP,[EBX+12]
  imul dword ESI,[EBX+12]
  imul dword EDI,[EBX+12]

  imul dword EAX,[12]
  imul dword ECX,[12]
  imul dword EDX,[12]
  imul dword EBX,[12]
  imul dword ESP,[12]
  imul dword EBP,[12]
  imul dword ESI,[12]
  imul dword EDI,[12]

  imul dword EAX,[123456789]
  imul dword ECX,[123456789]
  imul dword EDX,[123456789]
  imul dword EBX,[123456789]
  imul dword ESP,[123456789]
  imul dword EBP,[123456789]
  imul dword ESI,[123456789]
  imul dword EDI,[123456789]










  idiv dword EAX
  idiv dword ECX
  idiv dword EDX
  idiv dword EBX
  idiv dword ESP
  idiv dword EBP
  idiv dword ESI
  idiv dword EDI

  idiv dword [EAX]
  idiv dword [ECX]
  idiv dword [EDX]
  idiv dword [EBX]
  idiv dword [ESP]
  idiv dword [EBP]
  idiv dword [ESI]
  idiv dword [EDI]

  idiv dword [EAX+12]
  idiv dword [ECX+12]
  idiv dword [EDX+12]
  idiv dword [EBX+12]
  idiv dword [ESP+12]
  idiv dword [EBP+12]
  idiv dword [ESI+12]
  idiv dword [EDI+12]

  idiv dword [EAX+123456789]
  idiv dword [ECX+123456789]
  idiv dword [EDX+123456789]
  idiv dword [EBX+123456789]
  idiv dword [ESP+123456789]
  idiv dword [EBP+123456789]
  idiv dword [ESI+123456789]
  idiv dword [EDI+123456789]

  idiv dword [12]
  idiv dword [12]
  idiv dword [12]
  idiv dword [12]
  idiv dword [12]
  idiv dword [12]
  idiv dword [12]
  idiv dword [12]

  idiv dword [123456789]
  idiv dword [123456789]
  idiv dword [123456789]
  idiv dword [123456789]
  idiv dword [123456789]
  idiv dword [123456789]
  idiv dword [123456789]
  idiv dword [123456789]

  add dword ECX,[EAX+12+4*EBX]
  add dword ECX,[ECX+12+4*EBX]
  add dword ECX,[EDX+12+4*EBX]
  add dword ECX,[EBX+12+4*EBX]
  add dword ECX,[ESP+12+4*EBX]
  add dword ECX,[EBP+12+4*EBX]
  add dword ECX,[ESI+12+4*EBX]
  add dword ECX,[EDI+12+4*EBX]

  add dword ECX,[EAX+123456789+8*EDI]
  add dword ECX,[ECX+123456789+8*ESI]
  add dword ECX,[EDX+123456789+8*EBP]
  add dword ECX,[ESP+123456789+8*EBX]
  add dword ECX,[EBP+123456789+8*EDX]
  add dword ECX,[ESI+123456789+8*ECX]
  add dword ECX,[EDI+123456789+8*EAX]

  sub dword ECX,[EAX+12+4*EBX]
  sub dword ECX,[ECX+12+4*EBX]
  sub dword ECX,[EDX+12+4*EBX]
  sub dword ECX,[EBX+12+4*EBX]
  sub dword ECX,[ESP+12+4*EBX]
  sub dword ECX,[EBP+12+4*EBX]
  sub dword ECX,[ESI+12+4*EBX]
  sub dword ECX,[EDI+12+4*EBX]

  sub dword ECX,[EAX+123456789+8*EDI]
  sub dword ECX,[ECX+123456789+8*ESI]
  sub dword ECX,[EDX+123456789+8*EBP]
  sub dword ECX,[ESP+123456789+8*EBX]
  sub dword ECX,[EBP+123456789+8*EDX]
  sub dword ECX,[ESI+123456789+8*ECX]
  sub dword ECX,[EDI+123456789+8*EAX]

  imul dword ECX,[EAX+12+4*EBX]
  imul dword ECX,[ECX+12+4*EBX]
  imul dword ECX,[EDX+12+4*EBX]
  imul dword ECX,[EBX+12+4*EBX]
  imul dword ECX,[ESP+12+4*EBX]
  imul dword ECX,[EBP+12+4*EBX]
  imul dword ECX,[ESI+12+4*EBX]
  imul dword ECX,[EDI+12+4*EBX]

  imul dword ECX,[EAX+123456789+8*EDI]
  imul dword ECX,[ECX+123456789+8*ESI]
  imul dword ECX,[EDX+123456789+8*EBP]
  imul dword ECX,[ESP+123456789+8*EBX]
  imul dword ECX,[EBP+123456789+8*EDX]
  imul dword ECX,[ESI+123456789+8*ECX]
  imul dword ECX,[EDI+123456789+8*EAX]

  push dword [EAX+12+4*EBX]
  push dword [ECX+12+4*EBX]
  push dword [EDX+12+4*EBX]
  push dword [EBX+12+4*EBX]
  push dword [ESP+12+4*EBX]
  push dword [EBP+12+4*EBX]
  push dword [ESI+12+4*EBX]
  push dword [EDI+12+4*EBX]

  push dword [EAX+123456789+8*EDI]
  push dword [ECX+123456789+8*ESI]
  push dword [EDX+123456789+8*EBP]
  push dword [ESP+123456789+8*EBX]
  push dword [EBP+123456789+8*EDX]
  push dword [ESI+123456789+8*ECX]
  push dword [EDI+123456789+8*EAX]

  lea EBX,[EAX]
  lea EBX,[ECX]
  lea EBX,[EDX]
  lea EBX,[EBX]
  lea EBX,[ESP]
  lea EBX,[EBP]
  lea EBX,[ESI]
  lea EBX,[EDI]

  lea EAX,[EBX]
  lea ECX,[EBX]
  lea EDX,[EBX]
  lea EBX,[EBX]
  lea ESP,[EBX]
  lea EBP,[EBX]
  lea ESI,[EBX]
  lea EDI,[EBX]

  lea EBX,[12]
  lea EBX,[12]
  lea EBX,[12]
  lea EBX,[12]
  lea EBX,[12]
  lea EBX,[12]
  lea EBX,[12]
  lea EBX,[12]

  lea EBX,[123456789]
  lea EBX,[123456789]
  lea EBX,[123456789]
  lea EBX,[123456789]
  lea EBX,[123456789]
  lea EBX,[123456789]
  lea EBX,[123456789]
  lea EBX,[123456789]

  lea EBX,[EAX+12]
  lea EBX,[ECX+12]
  lea EBX,[EDX+12]
  lea EBX,[EBX+12]
  lea EBX,[ESP+12]
  lea EBX,[EBP+12]
  lea EBX,[ESI+12]
  lea EBX,[EDI+12]

  lea EBX,[EAX+123456789]
  lea EBX,[ECX+123456789]
  lea EBX,[EDX+123456789]
  lea EBX,[EBX+123456789]
  lea EBX,[ESP+123456789]
  lea EBX,[EBP+123456789]
  lea EBX,[ESI+123456789]
  lea EBX,[EDI+123456789]

  lea EBX,[EAX+12+4*EAX]
  lea EBX,[ECX+12+4*EAX]
  lea EBX,[EDX+12+4*EAX]
  lea EBX,[EBX+12+4*EAX]
  lea EBX,[ESP+12+4*EAX]
  lea EBX,[EBP+12+4*EAX]
  lea EBX,[ESI+12+4*EAX]
  lea EBX,[EDI+12+4*EAX]

  lea EBX,[EAX+123456789+8*EDI]
  lea EBX,[ECX+123456789+8*EDI]
  lea EBX,[EDX+123456789+8*EDI]
  lea EBX,[EBX+123456789+8*EDI]
  lea EBX,[ESP+123456789+8*EDI]
  lea EBX,[EBP+123456789+8*EDI]
  lea EBX,[ESI+123456789+8*EDI]
  lea EBX,[EDI+123456789+8*EDI]

  mov dword [12],0
  mov dword [12],12
  mov dword [12],123456789

  mov dword [123456789],0
  mov dword [123456789],12
  mov dword [123456789],123456789

  fld qword [EAX]
  fld qword [ECX]
  fld qword [EDX]
  fld qword [EBX]
  fld qword [ESP]
  fld qword [EBP]
  fld qword [ESI]
  fld qword [EDI]

  fld qword [EAX+12]
  fld qword [ECX+12]
  fld qword [EDX+12]
  fld qword [EBX+12]
  fld qword [ESP+12]
  fld qword [EBP+12]
  fld qword [ESI+12]
  fld qword [EDI+12]

  fld qword [EAX+123456789]
  fld qword [ECX+123456789]
  fld qword [EDX+123456789]
  fld qword [EBX+123456789]
  fld qword [ESP+123456789]
  fld qword [EBP+123456789]
  fld qword [ESI+123456789]
  fld qword [EDI+123456789]

  fld qword [EAX+12+4*EAX]
  fld qword [ECX+12+4*EAX]
  fld qword [EDX+12+4*EAX]
  fld qword [EBX+12+4*EAX]
  fld qword [ESP+12+4*EAX]
  fld qword [EBP+12+4*EAX]
  fld qword [ESI+12+4*EAX]
  fld qword [EDI+12+4*EAX]

  fld qword [EAX+123456789+8*EDI]
  fld qword [ECX+123456789+8*EDI]
  fld qword [EDX+123456789+8*EDI]
  fld qword [EBX+123456789+8*EDI]
  fld qword [ESP+123456789+8*EDI]
  fld qword [EBP+123456789+8*EDI]
  fld qword [ESI+123456789+8*EDI]
  fld qword [EDI+123456789+8*EDI]

  fld qword [12]
  fld qword [123456789]

  fld ST0
  fld ST1
  fld ST2
  fld ST3
  fld ST4
  fld ST5
  fld ST6
  fld ST7

  fst qword [EAX]
  fst qword [ECX+12]
  fst qword [EDX+123456789]
  fst qword [EBX+12+4*EAX]
  fst qword [ESP+123456789+8*EDI]
  fst qword [12]
  fst qword [123456789]

  fst ST0
  fst ST1
  fst ST2
  fst ST3
  fst ST4
  fst ST5
  fst ST6
  fst ST7

  fstp qword [EAX]
  fstp qword [ECX+12]
  fstp qword [EDX+123456789]
  fstp qword [EBX+12+4*EAX]
  fstp qword [ESP+123456789+8*EDI]
  fstp qword [12]
  fstp qword [123456789]

  fstp ST0
  fstp ST1
  fstp ST2
  fstp ST3
  fstp ST4
  fstp ST5
  fstp ST6
  fstp ST7

  fild dword [EAX]
  fild dword [ECX]
  fild dword [EDX]
  fild dword [EBX]
  fild dword [ESP]
  fild dword [EBP]
  fild dword [ESI]
  fild dword [EDI]

  fild dword [EAX+12]
  fild dword [ECX+12]
  fild dword [EDX+12]
  fild dword [EBX+12]
  fild dword [ESP+12]
  fild dword [EBP+12]
  fild dword [ESI+12]
  fild dword [EDI+12]

  fild dword [EAX+123456789]
  fild dword [ECX+123456789]
  fild dword [EDX+123456789]
  fild dword [EBX+123456789]
  fild dword [ESP+123456789]
  fild dword [EBP+123456789]
  fild dword [ESI+123456789]
  fild dword [EDI+123456789]

  fild dword [EAX+12+4*EAX]
  fild dword [ECX+12+4*EAX]
  fild dword [EDX+12+4*EAX]
  fild dword [EBX+12+4*EAX]
  fild dword [ESP+12+4*EAX]
  fild dword [EBP+12+4*EAX]
  fild dword [ESI+12+4*EAX]
  fild dword [EDI+12+4*EAX]

  fild dword [EAX+123456789+8*EDI]
  fild dword [ECX+123456789+8*EDI]
  fild dword [EDX+123456789+8*EDI]
  fild dword [EBX+123456789+8*EDI]
  fild dword [ESP+123456789+8*EDI]
  fild dword [EBP+123456789+8*EDI]
  fild dword [ESI+123456789+8*EDI]
  fild dword [EDI+123456789+8*EDI]

  fild dword [12]
  fild dword [123456789]

  fadd qword [EAX]
  fadd qword [ECX+12]
  fadd qword [EDX+123456789]
  fadd qword [EBX+12+4*EAX]
  fadd qword [ESP+123456789+8*EDI]
  fadd qword [12]
  fadd qword [123456789]

  fadd ST0
  fadd ST1
  fadd ST2
  fadd ST3
  fadd ST4
  fadd ST5
  fadd ST6
  fadd ST7

  fsub qword [EAX]
  fsub qword [ECX+12]
  fsub qword [EDX+123456789]
  fsub qword [EBX+12+4*EAX]
  fsub qword [ESP+123456789+8*EDI]
  fsub qword [12]
  fsub qword [123456789]

  fsub ST0
  fsub ST1
  fsub ST2
  fsub ST3
  fsub ST4
  fsub ST5
  fsub ST6
  fsub ST7

  fmul qword [EAX]
  fmul qword [ECX+12]
  fmul qword [EDX+123456789]
  fmul qword [EBX+12+4*EAX]
  fmul qword [ESP+123456789+8*EDI]
  fmul qword [12]
  fmul qword [123456789]

  fmul ST0
  fmul ST1
  fmul ST2
  fmul ST3
  fmul ST4
  fmul ST5
  fmul ST6
  fmul ST7

  fdiv qword [EAX]
  fdiv qword [ECX+12]
  fdiv qword [EDX+123456789]
  fdiv qword [EBX+12+4*EAX]
  fdiv qword [ESP+123456789+8*EDI]
  fdiv qword [12]
  fdiv qword [123456789]

  fdiv ST0
  fdiv ST1
  fdiv ST2
  fdiv ST3
  fdiv ST4
  fdiv ST5
  fdiv ST6
  fdiv ST7

  fucompp
  fnstsw ax
  sahf
  fsqrt
  int3
  pushf
  popf
