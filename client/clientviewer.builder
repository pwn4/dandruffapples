<?xml version="1.0" encoding="UTF-8"?>
<interface>
  <!-- interface-requires gtk+ 2.8 -->
  <!-- interface-naming-policy project-wide -->
  <object class="GtkWindow" id="window">
    <property name="visible">True</property>
    <property name="border_width">10</property>
    <property name="title" translatable="yes">World Viewer</property>
    <property name="default_width">425</property>
    <property name="default_height">250</property>
    <signal name="destroy" handler="gtk_main_quit"/>
    <signal name="delete_event" handler="gtk_main_quit"/>
    <child>
      <object class="GtkVBox" id="vbox">
        <property name="visible">True</property>
        <child>
          <object class="GtkToolbar" id="buttonToolbar">
            <property name="visible">True</property>
            <property name="toolbar_style">both</property>
            <child>
              <object class="GtkToolItem" id="toolbarCombo">
                <property name="visible">True</property>
                <child>
                  <object class="GtkComboBox" id="comboRobotId">
                    <property name="visible">True</property>
                  </object>
                </child>
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="homogeneous">True</property>
              </packing>
            </child>
            <child>
              <object class="GtkSeparatorToolItem" id="vsep">
                <property name="visible">True</property>
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="homogeneous">True</property>
              </packing>
            </child>
            <child>
              <object class="GtkToggleToolButton" id="Info">
                <property name="visible">True</property>
                <property name="label" translatable="yes">Info</property>
                <property name="use_underline">True</property>
                <property name="stock_id">gtk-info</property>
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="homogeneous">True</property>
              </packing>
            </child>
            <child>
              <object class="GtkToolButton" id="About">
                <property name="visible">True</property>
                <property name="label" translatable="yes">About</property>
                <property name="use_underline">True</property>
                <property name="stock_id">gtk-about</property>
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="homogeneous">True</property>
              </packing>
            </child>
            <child>
              <object class="GtkSeparatorToolItem" id="vsep1">
                <property name="visible">True</property>
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="homogeneous">True</property>
              </packing>
            </child>
            <child>
              <object class="GtkToolButton" id="Exit">
                <property name="visible">True</property>
                <property name="label" translatable="yes">Quit</property>
                <property name="use_underline">True</property>
                <property name="stock_id">gtk-quit</property>
                <signal name="clicked" handler="gtk_main_quit"/>
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="homogeneous">True</property>
              </packing>
            </child>
          </object>
          <packing>
            <property name="expand">False</property>
            <property name="position">0</property>
          </packing>
        </child>
        <child>
          <object class="GtkDrawingArea" id="area">
            <property name="visible">True</property>
          </object>
          <packing>
            <property name="position">1</property>
          </packing>
        </child>
      </object>
    </child>
  </object>
  <object class="GtkWindow" id="infoWindow">
    <property name="title" translatable="yes">Info</property>
    <child>
      <placeholder/>
    </child>
  </object>
</interface>
