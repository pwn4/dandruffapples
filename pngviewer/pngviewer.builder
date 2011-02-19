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
          <object class="GtkToolbar" id="buttonToolbar">
            <property name="visible">True</property>
            <property name="toolbar_style">both</property>
            <child>
              <object class="GtkToggleToolButton" id="Navigation">
                <property name="visible">True</property>
                <property name="sensitive">False</property>
                <property name="label" translatable="yes">Navigation</property>
                <property name="use_underline">True</property>
                <property name="stock_id">gtk-about</property>
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="homogeneous">True</property>
              </packing>
            </child>
            <child>
              <object class="GtkToggleToolButton" id="Info">
                <property name="visible">True</property>
                <property name="sensitive">False</property>
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
              <object class="GtkSeparatorToolItem" id="vsep">
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
          <object class="GtkScrolledWindow" id="scrolledWindow">
            <property name="visible">True</property>
            <property name="can_focus">True</property>
            <property name="hscrollbar_policy">automatic</property>
            <property name="vscrollbar_policy">automatic</property>
            <child>
              <object class="GtkViewport" id="viewport">
                <property name="visible">True</property>
                <child>
                  <object class="GtkTable" id="table">
                    <property name="visible">True</property>
                    <property name="n_rows">2</property>
                    <property name="n_columns">2</property>
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
                </child>
              </object>
            </child>
          </object>
          <packing>
            <property name="position">1</property>
          </packing>
        </child>
      </object>
    </child>
  </object>
  <object class="GtkWindow" id="navigationWindow">
    <property name="border_width">5</property>
    <property name="title"> </property>
    <property name="window_position">center-always</property>
    <property name="default_width">150</property>
    <property name="default_height">250</property>
    <signal name="destroy" handler="gtk_widget_hide"/>
    <signal name="delete_event" handler="gtk_widget_hide"/>
    <child>
      <object class="GtkVBox" id="vbox1">
        <property name="visible">True</property>
        <child>
          <object class="GtkTable" id="worldGrid">
            <property name="visible">True</property>
            <property name="n_rows">2</property>
            <property name="n_columns">2</property>
            <property name="column_spacing">1</property>
            <property name="row_spacing">1</property>
            <property name="homogeneous">True</property>
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
            <property name="padding">1</property>
            <property name="position">0</property>
          </packing>
        </child>
        <child>
          <object class="GtkTable" id="table1">
            <property name="visible">True</property>
            <property name="n_rows">3</property>
            <property name="n_columns">3</property>
            <child>
              <object class="GtkButton" id="up">
                <property name="visible">True</property>
                <property name="can_focus">True</property>
                <property name="receives_default">True</property>
                <property name="image">upImg</property>
              </object>
              <packing>
                <property name="left_attach">1</property>
                <property name="right_attach">2</property>
              </packing>
            </child>
            <child>
              <object class="GtkButton" id="back">
                <property name="visible">True</property>
                <property name="can_focus">True</property>
                <property name="receives_default">True</property>
                <property name="image">backImg</property>
                <property name="image_position">top</property>
              </object>
              <packing>
                <property name="top_attach">1</property>
                <property name="bottom_attach">2</property>
              </packing>
            </child>
            <child>
              <object class="GtkButton" id="forward">
                <property name="visible">True</property>
                <property name="can_focus">True</property>
                <property name="receives_default">True</property>
                <property name="image">forwardImg</property>
                <property name="image_position">top</property>
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
                <property name="visible">True</property>
                <property name="can_focus">True</property>
                <property name="receives_default">True</property>
                <property name="image">downImg</property>
                <property name="image_position">top</property>
              </object>
              <packing>
                <property name="left_attach">1</property>
                <property name="right_attach">2</property>
                <property name="top_attach">2</property>
                <property name="bottom_attach">3</property>
              </packing>
            </child>
            <child>
              <object class="GtkButton" id="rotate">
                <property name="visible">True</property>
                <property name="can_focus">True</property>
                <property name="receives_default">True</property>
                <property name="image">rotateImg</property>
                <property name="image_position">top</property>
              </object>
              <packing>
                <property name="left_attach">1</property>
                <property name="right_attach">2</property>
                <property name="top_attach">1</property>
                <property name="bottom_attach">2</property>
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
          </object>
          <packing>
            <property name="expand">False</property>
            <property name="fill">False</property>
            <property name="padding">1</property>
            <property name="position">1</property>
          </packing>
        </child>
      </object>
    </child>
  </object>
  <object class="GtkImage" id="forwardImg">
    <property name="visible">True</property>
    <property name="stock">gtk-go-forward</property>
  </object>
  <object class="GtkImage" id="downImg">
    <property name="visible">True</property>
    <property name="stock">gtk-go-down</property>
  </object>
  <object class="GtkImage" id="backImg">
    <property name="visible">True</property>
    <property name="stock">gtk-go-back</property>
  </object>
  <object class="GtkImage" id="upImg">
    <property name="visible">True</property>
    <property name="stock">gtk-go-up</property>
  </object>
  <object class="GtkWindow" id="infoWindow">
    <property name="title" translatable="yes"> </property>
    <property name="resizable">False</property>
    <property name="window_position">center-always</property>
    <property name="default_width">440</property>
    <property name="default_height">250</property>
    <child>
      <object class="GtkLabel" id="test">
        <property name="visible">True</property>
        <property name="label" translatable="yes">testtesttesttesttest</property>
      </object>
    </child>
  </object>
  <object class="GtkImage" id="rotateImg">
    <property name="visible">True</property>
    <property name="stock">gtk-refresh</property>
  </object>
</interface>
