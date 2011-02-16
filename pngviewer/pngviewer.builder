<?xml version="1.0" encoding="UTF-8"?>
<interface>
  <!-- interface-requires gtk+ 2.8 -->
  <!-- interface-naming-policy project-wide -->
  <object class="GtkWindow" id="window">
    <property name="visible">True</property>
    <property name="border_width">10</property>
    <property name="title" translatable="yes">Antix</property>
    <property name="default_width">1000</property>
    <property name="default_height">1000</property>
    <signal name="destroy" handler="gtk_main_quit"/>
    <signal name="delete_event" handler="gtk_main_quit"/>
    <child>
      <object class="GtkVBox" id="vbox">
        <property name="visible">True</property>
        <child>
          <object class="GtkToolbar" id="toolbar">
            <property name="visible">True</property>
            <property name="toolbar_style">both</property>
            <child>
              <object class="GtkToggleToolButton" id="Navigation">
                <property name="visible">True</property>
                <property name="label" translatable="yes">Navigation</property>
                <property name="use_underline">True</property>
                <property name="stock_id">gtk-about</property>
                <signal name="toggled" handler="on_Navigation_toggled"/>
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="homogeneous">True</property>
              </packing>
            </child>
            <child>
              <object class="GtkToggleToolButton" id="Properties">
                <property name="visible">True</property>
                <property name="label" translatable="yes">Properties</property>
                <property name="use_underline">True</property>
                <property name="stock_id">gtk-properties</property>
                <signal name="toggled" handler="on_Properties_toggled"/>
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
      </object>
    </child>
  </object>
  <object class="GtkWindow" id="navigation">
    <property name="type">popup</property>
    <property name="window_position">mouse</property>
    <property name="default_width">440</property>
    <property name="default_height">250</property>
    <child>
      <object class="GtkVBox" id="vbox1">
        <property name="visible">True</property>
        <child>
          <placeholder/>
        </child>
        <child>
          <object class="GtkTable" id="table1">
            <property name="visible">True</property>
            <property name="n_rows">3</property>
            <property name="n_columns">3</property>
            <child>
              <object class="GtkButton" id="up">
                <property name="label">gtk-go-up</property>
                <property name="visible">True</property>
                <property name="can_focus">True</property>
                <property name="receives_default">True</property>
                <property name="use_stock">True</property>
              </object>
              <packing>
                <property name="left_attach">1</property>
                <property name="right_attach">2</property>
              </packing>
            </child>
            <child>
              <object class="GtkButton" id="back">
                <property name="label">gtk-go-back</property>
                <property name="visible">True</property>
                <property name="can_focus">True</property>
                <property name="receives_default">True</property>
                <property name="use_stock">True</property>
              </object>
              <packing>
                <property name="top_attach">1</property>
                <property name="bottom_attach">2</property>
              </packing>
            </child>
            <child>
              <object class="GtkButton" id="forward">
                <property name="label">gtk-go-forward</property>
                <property name="visible">True</property>
                <property name="can_focus">True</property>
                <property name="receives_default">True</property>
                <property name="use_stock">True</property>
              </object>
              <packing>
                <property name="left_attach">2</property>
                <property name="right_attach">3</property>
                <property name="top_attach">1</property>
                <property name="bottom_attach">2</property>
              </packing>
            </child>
            <child>
              <object class="GtkButton" id="down">
                <property name="label">gtk-go-down</property>
                <property name="visible">True</property>
                <property name="can_focus">True</property>
                <property name="receives_default">True</property>
                <property name="use_stock">True</property>
              </object>
              <packing>
                <property name="left_attach">1</property>
                <property name="right_attach">2</property>
                <property name="top_attach">2</property>
                <property name="bottom_attach">3</property>
              </packing>
            </child>
            <child>
              <placeholder/>
            </child>
            <child>
              <placeholder/>
            </child>
            <child>
              <placeholder/>
            </child>
            <child>
              <placeholder/>
            </child>
            <child>
              <placeholder/>
            </child>
          </object>
          <packing>
            <property name="position">1</property>
          </packing>
        </child>
      </object>
    </child>
  </object>
</interface>
