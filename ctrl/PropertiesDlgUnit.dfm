object PropertiesDlg: TPropertiesDlg
  Left = 624
  Top = 451
  BorderStyle = bsDialog
  Caption = 'Control Panel Properties'
  ClientHeight = 198
  ClientWidth = 433
  Color = clBtnFace
  ParentFont = True
  OldCreateOrder = True
  Position = poScreenCenter
  PixelsPerInch = 120
  TextHeight = 16
  object Bevel1: TBevel
    Left = 10
    Top = 10
    Width = 306
    Height = 174
    Anchors = [akLeft, akTop, akRight, akBottom]
    Shape = bsFrame
  end
  object Label1: TLabel
    Left = 24
    Top = 88
    Width = 110
    Height = 24
    AutoSize = False
    Caption = 'Control Directory'
  end
  object Label2: TLabel
    Left = 24
    Top = 56
    Width = 110
    Height = 24
    AutoSize = False
    Caption = 'Config Command'
  end
  object Label3: TLabel
    Left = 24
    Top = 24
    Width = 110
    Height = 24
    AutoSize = False
    Caption = 'Login Command'
  end
  object Label4: TLabel
    Left = 24
    Top = 120
    Width = 225
    Height = 24
    AutoSize = False
    Caption = 'Node Display Interval (in seconds)'
  end
  object OKBtn: TButton
    Left = 329
    Top = 10
    Width = 93
    Height = 31
    Anchors = [akTop, akRight]
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 5
  end
  object CancelBtn: TButton
    Left = 329
    Top = 47
    Width = 93
    Height = 31
    Anchors = [akTop, akRight]
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 6
  end
  object CtrlDirEdit: TEdit
    Left = 136
    Top = 88
    Width = 169
    Height = 24
    TabOrder = 2
  end
  object ConfigCmdEdit: TEdit
    Left = 136
    Top = 56
    Width = 169
    Height = 24
    TabOrder = 1
  end
  object LoginCmdEdit: TEdit
    Left = 136
    Top = 24
    Width = 169
    Height = 24
    TabOrder = 0
  end
  object TrayIconCheckBox: TCheckBox
    Left = 24
    Top = 152
    Width = 281
    Height = 24
    Caption = 'Minimize to System Tray'
    TabOrder = 4
  end
  object NodeIntEdit: TEdit
    Left = 256
    Top = 120
    Width = 49
    Height = 24
    TabOrder = 3
  end
end
