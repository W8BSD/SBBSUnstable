/* text.js */

/* Synchronet static text string constants */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

/****************************************************************************/
/* Values for elements of the array of pointers (bbs.text()) to static text	*/
/* Should correlate with CTRL\TEXT.DAT										*/
/****************************************************************************/

var MsgSubj=0;
var MsgAttr=1;
var MsgTo=2;
var MsgToExt=3;
var MsgToNet=4;
var MsgFrom=5;
var MsgFromExt=6;
var MsgFromNet=7;
var MsgDate=8;
var Email=9;
var Post=10;
var TooManyEmailsToday=11;
var TooManyPostsToday=12;
var CantAddToQueue=13;
var R_Email=14;
var R_Post=15;
var R_Feedback=16;
var Emailing=17;
var Posting=18;
var NewFile=19;
var SubjectPrompt=20;
var EnterMsgNow=21;
var EnterMsgNowRaw=22;
var NoMoreLines=23;
var OnlyNLinesLeft=24;
var OutOfBytes=25;
var InvalidLineNumber=26;
var MsgCleared=27;
var WithLineNumbersQ=28;
var Aborted=29;
var AnonymousQ=30;
var Anonymous=31;
var Saving=32;
var SavedNBytes=33;
var WritingIndx=34;
var Emailed=35;
var Posted=36;
var EmailNodeMsg=37;
var R_Forward=38;
var ForwardedFrom=39;
var Forwarded=40;
var AutoMsgBy=41;
var AutoMsg=42;
var R_AutoMsg=43;
var NoMailWaiting=44;
var MailWaitingLstHdr=45;
var MailWaitingLstFmt=46;
var StartWithN=47;
var ReadingMail=48;
var CantReplyToAnonMsg=49;
var Regarding=50;
var RegardingByOn=51;
var RegardingByToOn=52;
var DeleteMailQ=53;
var OK=54;
var ForwardMailTo=55;
var SaveMsgToFile=56;
var FileToWriteTo=57;
var NoMailSent=58;
var MailSentLstHdr=59;
var ReadingSentMail=60;
var InternetMailing=61;
var NoMailOnSystem=62;
var ReadingAllMail=63;
var MailOnSystemLstHdr=64;
var MailOnSystemLstFmt=65;
var NScanStatusFmt=66;
var SearchSubFmt=67;
var NoMsgsOnSub=68;
var ZScanPostHdr=69;
var ReadingSub=70;
var YouDidntPostMsgN=71;
var DeletePostQ=72;
var UserDefaultsAutoLogon=73;
var MsgSentToUser=74;
var SearchStringPrompt=75;
var PrivateChatSeparator=76;
var SubMsgLstFmt=77;
var SysopChatSeparator=78;
var NoTextSections=79;
var TextSectionLstHdr=80;
var TextSectionLstFmt=81;
var WhichTextSection=82;
var TextFilesLstHdr=83;
var TextFilesLstFmt=84;
var WhichTextFileSysop=85;
var WhichTextFile=86;
var AddTextFileBeforeWhich=87;
var AddTextFilePath=88;
var AddTextFileDesc=89;
var RemoveWhichTextFile=90;
var DeleteTextFileQ=91;
var EditWhichTextFile=92;
var NScanAllGrpsQ=93;
var SScanAllGrpsQ=94;
var AreYouSureQ=95;
var R_Chat=96;
var ReFeedback=97;
var SiHdr=98;
var SiSysName=99;
var SiSysID=100;
var SiSysFaddr=101;
var SiSysPsite=102;
var SiSysLocation=103;
var SiSysop=104;
var SiSysNodes=105;
var SiNodeNumberName=106;
var SiNodePhone=107;
var SiTotalLogons=108;
var SiLogonsToday=109;
var SiTotalTime=110;
var SiTimeToday=111;
var ViewSysInfoFileQ=112;
var ViewLogonMsgQ=113;
var R_ReadSentMail=114;
var MessageScan=115;
var MessageScanComplete=116;
var MessageScanAborted=117;
var LogOffQ=118;
var CantPostOnSub=119;
var SendingMessageToUser=120;
var NoOtherActiveNodes=121;
var R_ExternalPrograms=122;
var UserStats=123;
var SubLstHdr=124;
var SubLstFmt=125;
var GrpLstHdr=126;
var GrpLstFmt=127;
var NScanCfgWhichGrp=128;
var SScanCfgWhichGrp=129;
var CfgSubLstHdr=130;
var CfgSubLstFmt=131;
var CfgGrpLstHdr=132;
var CfgGrpLstFmt=133;
var NScanCfgWhichSub=134;
var SScanCfgWhichSub=135;
var SubPtrLstFmt=136;
var WhichOrAll=137;
var RawMsgInputModeIsNow=138;
var OFF=139;
var ON=140;
var PagingUser=141;
var SystemStatsHdr=142;
var NodeStatsHdr=143;
var StatsTotalLogons=144;
var StatsLogonsToday=145;
var StatsTotalTime=146;
var StatsTimeToday=147;
var StatsUploadsToday=148;
var StatsDownloadsToday=149;
var StatsPostsToday=150;
var StatsEmailsToday=151;
var StatsFeedbacksToday=152;
var ChUserPrompt=153;
var ChUserPwPrompt=154;
var Unused156=155;
var ErrorLogHdr=156;
var DeleteErrorLogQ=157;
var NoErrorLogExists=158;
var ClearErrCounter=159;
var InvalidNode=160;
var SendingTelegramToUser=161;
var DeleteGuruLogQ=162;
var TelegramFmt=163;
var R_Download=164;
var SearchingAllDirs=165;
var SearchingAllLibs=166;
var NFilesListed=167;
var EmptyDir=168;
var NScanHdr=169;
var R_RemoveFiles=170;
var DirFull=171;
var R_Upload=172;
var CantUploadHere=173;
var FileSpec=174;
var SystemPassword=175;
var NoSysopDir=176;
var CantUploadToSysop=177;
var DirLstHdr=178;
var DirLstFmt=179;
var LibLstHdr=180;
var LibLstFmt=181;
var InvalidNetMailAddr=182;
var TransferPolicyHdr=183;
var TransferProtLstFmt=184;
var TpUpload=185;
var TpDownload=186;
var NoUserDir=187;
var NoFilesForYou=188;
var UserDirFull=189;
var CantUploadToUser=190;
var BoxHdrLib=191;
var BoxHdrDir=192;
var BoxHdrFiles=193;
var ShortHdrLib=194;
var ShortHdrDir=195;
var BatchDlFlags=196;
var BatchDlQueueIsFull=197;
var FileSpecStarDotStar=198;
var LowDiskSpace=199;
var DiskNBytesFree=200;
var Filename=201;
var BadFilename=202;
var UploadToSysopDirQ=203;
var UploadToUserDirQ=204;
var UploadToCurDirQ=205;
var FileAlreadyThere=206;
var FileOnDiskAddQ=207;
var FileNotOnDisk=208;
var TheseFileExtsOnly=209;
var FileAlreadyOnline=210;
var EnterAfterLastDestUser=211;
var SendFileToUser=212;
var DuplicateUser=213;
var UserWontBeAbleToDl=214;
var CantSendYourselfFiles=215;
var UserAddedToDestList=216;
var RateThisFile=217;
var Rated=218;
var MultipleDiskQ=219;
var HowManyDisksTotal=220;
var NumberOfFile=221;
var FileOneOfTen=222;
var FileOneOfTwo=223;
var EnterDescNow=224;
var NoDescription=225;
var ProtocolOrQuit=226;
var ProtocolBatchOrQuit=227;
var BatchUlQueueIsFull=228;
var FileAddedToUlQueue=229;
var UserToUserXferNodeMsg=230;
var FileInfoPrompt=231;
var QuitOrNext=232;
var RExemptRemoveFilePrompt=233;
var MoveToLibLstFmt=234;
var MoveToLibPrompt=235;
var MoveToDirLstFmt=236;
var MoveToDirPrompt=237;
var MovedFile=238;
var CloseFileRecordQ=239;
var SysopRemoveFilePrompt=240;
var UserRemoveFilePrompt=241;
var FileNotThere=242;
var CouldntRemoveFile=243;
var DeleteFileQ=244;
var AddToOfflineDirQ=245;
var RemoveCreditsQ=246;
var UserNotFound=247;
var CreditsToRemove=248;
var FileRemovedUserMsg=249;
var EditFilename=250;
var CouldntRenameFile=251;
var FileRenamed=252;
var EditDescription=253;
var DeleteExtDescriptionQ=254;
var EditUploader=255;
var EditCreditValue=256;
var EditTimesDownloaded=257;
var EditOpenCount=258;
var EditAltPath=259;
var YouOnlyHaveNCredits=260;
var NotEnoughCredits=261;
var NotEnoughTimeToDl=262;
var ProtocolBatchQuitOrNext=263;
var BulkUpload=264;
var BulkUploadDescPrompt=265;
var NoFilesInBatchQueue=266;
var BatchMenuPrompt=267;
var ClearUploadQueueQ=268;
var UploadQueueCleared=269;
var ClearDownloadQueueQ=270;
var DownloadQueueCleared=271;
var DownloadQueueIsEmpty=272;
var UploadQueueLstHdr=273;
var UploadQueueLstFmt=274;
var DownloadQueueLstHdr=275;
var DownloadQueueLstFmt=276;
var DownloadQueueTotals=277;
var RemoveWhichFromUlQueue=278;
var RemoveWhichFromDlQueue=279;
var UploadQueueIsEmpty=280;
var HangUpAfterXferQ=281;
var StartXferNow=282;
var Disconnecting=283;
var Disconnected=284;
var FileNotSent=285;
var RemovingTempFiles=286;
var ExtractFrom=287;
var UnextractableFile=288;
var FileNotFound=289;
var ExtractFilesPrompt=290;
var TempDirPrompt=291;
var TempFileNotCreatedYet=292;
var TempFileInfo=293;
var TempDirTotal=294;
var NFilesRemoved=295;
var ResortWarning=296;
var ResortLineFmt=297;
var ResortEmptyDir=298;
var Sorting=299;
var Sorted=300;
var Compressed=301;
var FileAlreadyInQueue=302;
var FileIsNotOnline=303;
var FileAddedToBatDlQueue=304;
var NonviewableFile=305;
var FileNotReceived=306;
var FileHadErrors=307;
var FileZeroLength=308;
var FileNBytesReceived=309;
var FileNBytesSent=310;
var DownloadUserMsg=311;
var Partially=312;
var FiLib=313;
var FiDir=314;
var FiFilename=315;
var FiFileSize=316;
var FiCredits=317;
var FiDescription=318;
var FiUploadedBy=319;
var FiFileDate=320;
var FiDateUled=321;
var FiDateDled=322;
var FiTimesDled=323;
var FiTransferTime=324;
var FiAlternatePath=325;
var InvalidAlternatePathN=326;
var FileIsOpen=327;
var HappyBirthday=328;
var TimeToChangePw=329;
var NewPasswordQ=330;
var NewPassword=331;
var VerifyPassword=332;
var Wrong=333;
var PasswordChanged=334;
var NoMoreLogons=335;
var R_Logons=336;
var EnterYourAlias=337;
var EnterYourRealName=338;
var EnterYourCompany=339;
var EnterYourHandle=340;
var EnterYourSex=341;
var EnterYourAddress=342;
var EnterYourPhoneNumber=343;
var EnterYourBirthday=344;
var EnterYourCityState=345;
var EnterYourZipCode=346;
var EnterYourComputer=347;
var CallingFromNorthAmericaQ=348;
var UserInfoCorrectQ=349;
var LiUserNumberName=350;
var LiLogonsToday=351;
var LiTimeonToday=352;
var LiMailWaiting=353;
var LiSysopIs=354;
var LiSysopAvailable=355;
var LiSysopNotAvailable=356;
var UserOnTwoNodes=357;
var CriticalErrors=358;
var UserXferForYou=359;
var UnreceivedUserXfer=360;
var ReadYourMailNowQ=361;
var NoNewUsers=362;
var NewUserPasswordPrompt=363;
var AutoTerminalQ=364;
var AnsiTerminalQ=365;
var ColorTerminalQ=366;
var RipTerminalQ=367;
var ExAsciiTerminalQ=368;
var YouCantUseThatName=369;
var YourPasswordIs=370;
var NewUserPasswordVerify=371;
var IncorrectPassword=372;
var MagicWordPrompt=373;
var FailedMagicWord=374;
var SystemFull=375;
var NewUserFeedbackHdr=376;
var NoFeedbackWarning=377;
var NoXtrnPrograms=378;
var XtrnProgLstHdr=379;
var XtrnProgLstTitles=380;
var XtrnProgLstUnderline=381;
var XtrnProgLstFmt=382;
var WhichXtrnProg=383;
var UserRunningXtrn=384;
var RemoveNodeLockQ=385;
var MinimumModemSpeed=386;
var NoNodeAccess=387;
var NodeLocked=388;
var UnknownUser=389;
var InvalidLogon=390;
var SlogFmt=391;
var Locally=392;
var SortAlphaQ=393;
var CheckingSlots=394;
var UserListFmt=395;
var NTotalUsers=396;
var NUsersOnCurSub=397;
var NUsersOnCurDir=398;
var NScanDate=399;
var NScanYear=400;
var NScanMonth=401;
var NScanDay=402;
var NScanHour=403;
var NScanMinute=404;
var NScanPmQ=405;
var NScanAmQ=406;
var PasswordTooShort=407;
var PasswordNotChanged=408;
var PasswordInvalid=409;
var PasswordObvious=410;
var ComputerTypeMenu=411;
var ComputerTypePrompt=412;
var ComputerTypeB=413;
var ComputerTypeC=414;
var ComputerTypeD=415;
var ComputerTypeE=416;
var NoUserData=417;
var Deleted=418;
var Inactive=419;
var UeditAliasPassword=420;
var UeditRealNamePhone=421;
var UeditAddressBirthday=422;
var UeditLocationZipcode=423;
var UeditNoteHandle=424;
var UeditComputerModem=425;
var UeditCommentLine=426;
var UserDates=427;
var UserTimes=428;
var UserLogons=429;
var UserEmails=430;
var UserNetMail=431;
var UserUploads=432;
var UserDownloads=433;
var UserLeech=434;
var UserCredits=435;
var UserMinutes=436;
var UeditSecLevel=437;
var UeditFlags=438;
var UeditExempts=439;
var UeditPrompt=440;
var UeditRestoreQ=441;
var UeditActivateQ=442;
var UeditDeleteQ=443;
var UeditReadUserMailWQ=444;
var UeditReadUserMailSQ=445;
var UeditDeactivateUserQ=446;
var ChangeExemptionQ=447;
var FlagEditing=448;
var GoToUser=449;
var UeditLastOn=450;
var UeditFirstOn=451;
var UeditExpire=452;
var UeditPwModDate=453;
var UeditML=454;
var UeditNote=455;
var UeditComment=456;
var UeditUlBytes=457;
var UeditUploads=458;
var UeditDlBytes=459;
var UeditDownloads=460;
var UeditLeech=461;
var QuickValidateFmt=462;
var QuickValidatePrompt=463;
var UeditPassword=464;
var UeditCredits=465;
var UeditMinutes=466;
var UeditCopyUserQ=467;
var UeditCopyUserToSlot=468;
var ChangeRestrictsQ=469;
var ModifyCredits=470;
var ModifyMinutes=471;
var DeleteQuestionaireQ=472;
var UserDefaultsHdr=473;
var UserDefaultsTerminal=474;
var UserDefaultsXeditor=475;
var UserDefaultsRows=476;
var UserDefaultsMenuMode=477;
var UserDefaultsPause=478;
var UserDefaultsHotKey=479;
var UserDefaultsCursor=480;
var UserDefaultsCLS=481;
var UserDefaultsAskNScan=482;
var UserDefaultsAskSScan=483;
var UserDefaultsANFS=484;
var UserDefaultsRemember=485;
var UserDefaultsBatFlag=486;
var UserDefaultsNetMail=487;
var UserDefaultsCommandSet=488;
var UserDefaultsQuiet=489;
var UserDefaultsPassword=490;
var UserDefaultsArcType=491;
var UserDefaultsProtocol=492;
var UserDefaultsWhich=493;
var On=494;
var Off=495;
var HowManyRows=496;
var CurrentPassword=497;
var ForwardMailQ=498;
var EnterNetMailAddress=499;
var SelectItemHdr=500;
var SelectItemFmt=501;
var SelectItemWhich=502;
var SysopIsHere=503;
var EndOfChat=504;
var ChatPrompt=505;
var AnonUserChatHandle=506;
var WelcomeToMultiChat=507;
var WelcomeToChannelN=508;
var NodeInMultiChatLocally=509;
var YoureOnTheAir=510;
var NodeJoinedMultiChat=511;
var NodeLeftMultiChat=512;
var MultiChatCommandPrompt=513;
var PasswordProtected=514;
var CorrectPassword=515;
var WrongPassword=516;
var PasswordProtectChanQ=517;
var PasswordPrompt=518;
var ChatLineFmt=519;
var SysopPageIsNow=520;
var SysopIsNotAvailable=521;
var ChatWithGuruInsteadQ=522;
var PrivateMsgPrompt=523;
var NodeToPrivateChat=524;
var NodeNAlreadyInPChat=525;
var NodeNIsNotInUse=526;
var NoNeedToPageSelf=527;
var CantPageNode=528;
var NodePageMsg=529;
var AllNodePageMsg=530;
var NodePChatPageMsg=531;
var R_SendMessages=532;
var NodeToSendMsgTo=533;
var NoNeedToSendMsgToSelf=534;
var NodeMsgPrompt=535;
var NodeMsgFmt=536;
var AllNodeMsgFmt=537;
var PagingGuru=538;
var WaitingForNodeInPChat=539;
var NodeJoinedPrivateChat=540;
var NodeLeftPrivateChat=541;
var NoOneHasLoggedOnToday=542;
var Unused544=543;	/* was LastFewCallers */
var LastFewCallersFmt=544;
var CallersToday=545;
var DoYouMeanThisUserQ=546;
var UNKNOWN_USER=547;
var TimesUp=548;
var NodeLoggedOff=549;
var NodeLoggedOnAtNbps=550;
var TiLogon=551;
var TiNow=552;
var TiTimeon=553;
var TiTimeLeft=554;
var ControlKeyMenu=555;
var TakenTooLongToLogon=556;
var CallBackWhenYoureThere=557;
var YesNoQuestion=558;
var Yes=559;
var No=560;
var NoYesQuestion=561;
var Pause=562;
var ContinueQ=563; /* was Wait */
var SysStatsLogHdr=564;
var NodeStatsLogHdr=565;
var ReducedTime=566;
var EventInfo=567;
var UploadBeforeEvent=568;
var QWKPrompt=569;
var QWKCtrlACodes=570;
var QWKPackingSubboard=571;
var QWKPackedSubboard=572;
var QWKPackingEmail=573;
var QWKPackedEmail=574;
var QWKUnpacking=575;
var QWKUnpacked=576;
var QWKNoNewMessages=577;
var QWKCompressionFailed=578;
var QWKExtractionFailed=579;
var QWKReplyNotReceived=580;
var QWKInvalidConferenceN=581;
var CreatingFileList=582;
var CreatedFileList=583;
var NoFiles=584;
var MsgPtrsInitialized=585;
var ConversionRate=586;
var CreditsToMin=587;
var YouHaveTooManyMinutes=588;
var BillingNodeMsg=589;
var Convert100ktoNminQ=590;
var CreditedAccount=591;
var ANSICaptureIsNow=592;
var RetrievingFile=593;
var AltULPathIsNow=594;
var PrivatePostQ=595;
var PostTo=596;
var NoToUser=597;
var UsingRealName=598;
var PostingPrivately=599;
var PostingAnonymously=600;
var CantDeletePosts=601;
var SubInfoHdr=602;
var SubInfoLongName=603;
var SubInfoShortName=604;
var SubInfoQWKName=605;
var SubInfoMaxMsgs=606;
var SubInfoTagLine=607;
var SubInfoFidoNet=608;
var SubInfoViewFileQ=609;
var DirInfoHdr=610;
var DirInfoLongName=611;
var DirInfoShortName=612;
var DirInfoAllowedExts=613;
var DirInfoMaxFiles=614;
var DirInfoViewFileQ=615;
var NoNetMailAllowed=616;
var NetMailCostContinueQ=617;
var NetMailing=618;
var RemoveFromNewScanQ=619;
var SubGroupOrAll=620;
var DirLibOrAll=621;
var EnterPath=622;
var SearchExtendedQ=623;
var DisplaySubjectsOnlyQ=624;
var EchoIsNow=625;
var WelcomeToPrivateChat=626;
var UploadingREP=627;
var ReceivedFileViaQWK=628;
var QWKmsgLimitReached=629;
var PrivatePostsNotAllowed=630;
var LoadingMsgPtrs=631;
var LoadedMsgPtrs=632;
var QuoteMessageQ=633;
var QuoteLinesPrompt=634;
var ChatChanLstHdr=635;
var ChatChanLstTitles=636;
var ChatChanLstUnderline=637;
var ChatChanLstFmt=638;
var CantAccessThatChannel=639;
var CantDownloadFromDir=640;
var SearchingForDupes=641;
var SearchedForDupes=642;
var AccountWillExpireInNDays=643;
var AccountHasExpired=644;
var DownloadBatchQ=645;
var WaitingForDeviceN=646;
var UserSentYouMail=647;
var UserSentYouFile=648;
var UserReadYourMail=649;
var UserReadYourMailNodeMsg=650;
var JoinWhichGrp=651;
var JoinWhichSub=652;
var JoinWhichLib=653;
var JoinWhichDir=654;
var CfgDirLstHdr=655;
var CfgDirLstFmt=656;
var CfgLibLstHdr=657;
var CfgLibLstFmt=658;
var BatchFlagPrompt=659;
var FileListBatchCommands=660;
var DownloadAttachedFileQ=661;
var FreeMinLeft=662;
var FreeMinToDeposit=663;
var EmailFilesNotAllowed=664;
var CantRunThatProgram=665;
var OnlyXminutesLeft=666;
var AreYouThere=667;
var NoAccessLevel=668;
var NoAccessAge=669;
var NoAccessBPS=670;
var NoAccessCredit=671;
var NoAccessNode=672;
var NoAccessUser=673;
var NoAccessExpire=674;
var NoAccessTimeLeft=675;
var NoAccessTimeUsed=676;
var NoAccessTime=677;
var NoAccessPCR=678;
var NoAccessUDR=679;
var NoAccessUDFR=680;
var NoAccessFlag1=681;
var NoAccessFlag2=682;
var NoAccessFlag3=683;
var NoAccessFlag4=684;
var NoAccessSex=685;
var NoAccessExempt=686;
var NoAccessRest=687;
var NoAccessDay=688;
var NoAccessGroup=689;
var NoAccessSub=690;
var NoAccessLib=691;
var NoAccessDir=692;
var NodeLstHdr=693;
var NodeActionMain=694;
var NodeActionReadMsgs=695;
var NodeActionReadMail=696;
var NodeActionSendMail=697;
var NodeActionReadTxt=698;
var NodeActionReadSentMail=699;
var NodeActionPostMsg=700;
var NodeActionAutoMsg=701;
var NodeActionXtrn=702;
var NodeActionDefaults=703;
var NodeActionXfer=704;
var NodeActionDLing=705;
var NodeActionULing=706;
var NodeActionBiXfer=707;
var NodeActionListFiles=708;
var NodeActionLoggingOn=709;
var NodeActionLocalChat=710;
var NodeActionMultiChat=711;
var NodeActionGuruChat=712;
var NodeActionChatSec=713;
var NodeActionSysopAct=714;
var NodeActionQWK=715;
var NodeActionPrivateChat=716;
var NodeActionPaging=717;
var NodeActionRetrieving=718;
var YN=719;

var TOTAL_TEXT=720; 
