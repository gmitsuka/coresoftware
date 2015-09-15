C
C
C	THIS FUNCTION IS TO CALCULATE THE NUCLEAR PROFILE FUNCTION
C       OF THE  COLLIDERING SYSTEM (IN UNITS OF 1/mb)
C
	FUNCTION  PROFILE(XB)
        COMMON/PACT/BB,B1,PHI,Z1
        SAVE  /PACT/
        COMMON/HIPARNT/HIPR1(100),IHPR2(50),HINT1(100),IHNT2(50)
        SAVE  /HIPARNT/
	EXTERNAL FLAP, FLAP1, FLAP2
C
        BB=XB
        PROFILE=1.0
        IF(IHNT2(1).GT.1 .AND. IHNT2(3).GT.1) THEN
           PROFILE=float(IHNT2(1))*float(IHNT2(3))*0.1*
     &          GAUSS1(FLAP,0.0,HIPR1(34),0.01)
        ELSE IF(IHNT2(1).EQ.1 .AND. IHNT2(3).GT.1) THEN
           PROFILE=0.2*float(IHNT2(3))*
     &          GAUSS1(FLAP2,0.0,HIPR1(35),0.001)
        ELSE IF(IHNT2(1).GT.1 .AND. IHNT2(3).EQ.1) THEN
           PROFILE=0.2*float(IHNT2(1))*
     &          GAUSS1(FLAP1,0.0,HIPR1(34),0.001)
        ENDIF
	RETURN
	END