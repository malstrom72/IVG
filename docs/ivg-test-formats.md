# IVG Test Formats

This table lists IVG test files, their format version, and a brief note on the feature exercised.

Entries with `N/A` do not declare a format version.

**Feature tokens**

| Token   | Meaning                                |
| ------- | -------------------------------------- |
| arc     | arc commands                           |
| ctx     | context state operations               |
| ell     | ellipse commands                       |
| grad    | gradients                               |
| img     | image drawing                           |
| line    | line commands                          |
| loop    | loops                                   |
| macro   | macros                                  |
| mask    | masking                                 |
| pat     | pattern fills                           |
| path    | path drawing                            |
| pen     | pen options (dash, joints)             |
| poly    | polygon commands                       |
| star    | star commands                           |
| text    | text rendering                          |
| unicode | Unicode text                            |


| Test file                                                                            | Format | Features                             |
| ------------------------------------------------------------------------------------ | ----- | ------------------------------------ |
| tests/ivg/StarNewSyntax.ivg                                                          | IVG-3 | star                                 |
| tests/ivg/StarTest.ivg                                                               | IVG-3 | star+macro+loop+path                 |
| tests/ivg/StarTest2.ivg                                                              | IVG-3 | star+macro+loop+path                 |
| tests/ivg/arcEdgeCases.ivg                                                           | IVG-3 | arc+macro+ctx+path                   |
| tests/ivg/beatrick.ivg                                                               | IVG-3 | grad+ctx+path                        |
| tests/ivg/beatrick2.ivg                                                              | IVG-3 | grad+loop+ctx+path                   |
| tests/ivg/beatrickComb_logo.ivg                                                      | IVG-1 | grad+ctx+path                        |
| tests/ivg/beatrickComp_logo.ivg                                                      | IVG-1 | grad+ctx+path                        |
| tests/ivg/beatrickShort_logo.ivg                                                     | IVG-1 | grad+ctx+path                        |
| tests/ivg/beatrick_logo.ivg                                                          | IVG-1 | grad+ctx+path                        |
| tests/ivg/benderComp_logo.ivg                                                        | IVG-1 | grad+path                            |
| tests/ivg/bender_logo.ivg                                                            | IVG-1 | grad+text+path                       |
| tests/ivg/bitboxComp_logo.ivg                                                        | IVG-1 | macro+loop+ctx+path                  |
| tests/ivg/bitbox_logo.ivg                                                            | IVG-1 | macro+loop+ctx+text+path             |
| tests/ivg/definePathTest.ivg                                                         | IVG-3 | star+path                            |
| tests/ivg/ellipseSeparateRadii.ivg                                                   | IVG-1 | ell                                  |
| tests/ivg/ellipseSingleRadius.ivg                                                    | IVG-1 | ell                                  |
| tests/ivg/ellipseTest.ivg                                                            | IVG-1 | ell                                  |
| tests/ivg/externalImageTest.ivg                                                      | IVG-3 | img                 |
| tests/ivg/fillTransformTest.ivg                                                      | IVG-1 | ell+grad+pat+ctx                     |
| tests/ivg/fitTextTest.ivg                                                            | IVG-2 | macro+ctx+text                       |
| tests/ivg/flakesMin_logo.ivg                                                         | IVG-1 | grad+path                            |
| tests/ivg/flakes_logo.ivg                                                            | IVG-1 | grad+text+path                       |
| tests/ivg/fooBarMin_logo.ivg                                                         | IVG-1 | grad+pat+mask+ctx+text+path          |
| tests/ivg/fooBar_logo - barry.ivg                                                    | N/A   | grad+pat+mask+ctx+text+path          |
| tests/ivg/fooBar_logo - barry2.ivg                                                   | N/A   | grad+pat+mask+ctx+text+path          |
| tests/ivg/fooBar_logo - droppy.ivg                                                   | N/A   | grad+pat+mask+ctx+text+path          |
| tests/ivg/fooBar_logo - glitch1.ivg                                                  | N/A   | grad+mask+ctx+text+path              |
| tests/ivg/fooBar_logo.ivg                                                            | IVG-1 | grad+pat+mask+ctx+text+path          |
| tests/ivg/gammaTest.ivg                                                              | IVG-3 | ell+loop+ctx+text+path               |
| tests/ivg/gradientSpacingTest.ivg                                                    | IVG-3 | grad                                 |
| tests/ivg/gradientXFormTest2.ivg                                                     | N/A   | grad+pat+ctx+path                    |
| tests/ivg/gradientXFormTests.ivg                                                     | N/A   | ell+grad+path                        |
| tests/ivg/huge.ivg                                                                   | N/A   | star+macro+loop+path                 |
| tests/ivg/imageAlignTest.ivg                                                         | IVG-2 | ell+img+loop+text                    |
| tests/ivg/imageTest1.ivg                                                             | IVG-2 | ell+grad+pat+mask+img+macro+loop+ctx |
| tests/ivg/imageTest2.ivg                                                             | IVG-2 | ell+grad+pat+mask+img                |
| tests/ivg/js80rmx_logo.ivg                                                           | IVG-1 | pat+macro+loop+ctx+path              |
| tests/ivg/js80rmx_logo_full.ivg                                                      | IVG-1 | pat+macro+loop+ctx+text+path         |
| tests/ivg/linePenOptions.ivg                                                         | IVG-3 | line+pen            |
| tests/ivg/linePolygonTest.ivg                                                        | IVG-3 | line+poly           |
| tests/ivg/linearGradientSpaced.ivg                                                   | IVG-3 | grad                                 |
| tests/ivg/maskImageTest.ivg                                                          | IVG-2 | ell+mask+img+loop                    |
| tests/ivg/maskedPatternFillTest.ivg                                                  | N/A   | ell+pat+mask                         |
| tests/ivg/masktest.ivg                                                               | IVG-1 | star+ell+grad+mask+ctx               |
| tests/ivg/masktest2.ivg                                                              | IVG-1 | ell+grad+pat+mask+ctx                |
| tests/ivg/miterBugRegression.ivg                                                     | N/A   | path                                 |
| tests/ivg/mozaikComp_logo.ivg                                                        | IVG-1 | path                                 |
| tests/ivg/mozaik_logo.ivg                                                            | IVG-1 | path                                 |
| tests/ivg/pathInstructions.ivg                                                       | IVG-3 | arc+path                             |
| tests/ivg/pathStrokePenOptions.ivg                                                   | IVG-3 | path+pen            |
| tests/ivg/patterntest.ivg                                                            | IVG-1 | ell+grad+pat+mask                    |
| tests/ivg/patterntest2.ivg                                                           | IVG-1 | ell+grad+pat                         |
| tests/ivg/patterntest3.ivg                                                           | N/A   | ell+pat                              |
| tests/ivg/polygonPenOptions.ivg                                                      | IVG-3 | poly+pen            |
| tests/ivg/pongComp_logo.ivg                                                          | IVG-1 | path                                 |
| tests/ivg/pong_logo.ivg                                                              | IVG-1 | text+path                            |
| tests/ivg/radialGradientSpaced.ivg                                                   | IVG-3 | grad                                 |
| tests/ivg/reciterComp_logo.ivg                                                       | IVG-1 | grad+path                            |
| tests/ivg/reciter_logo.ivg                                                           | IVG-1 | grad+path                            |
| tests/ivg/regtest1.ivg                                                               | IVG-2 | grad+macro+loop                      |
| tests/ivg/resetTest.ivg                                                              | IVG-1 | ctx                                  |
| tests/ivg/rightEdgeTest.ivg                                                          | IVG-1 |                                      |
| tests/ivg/ringmodComp_logo.ivg                                                       | IVG-1 | ell+path                             |
| tests/ivg/ringmod_logo.ivg                                                           | IVG-1 | ell+loop+path                        |
| tests/ivg/specular_logo.ivg                                                          | IVG-1 | ell+grad+macro+text+path             |
| tests/ivg/starCommandTest.ivg                                                        | IVG-1 | star                                 |
| tests/ivg/test.ivg                                                                   | IVG-3 | ell+macro+loop+ctx+path              |
| tests/ivg/test2.ivg                                                                  | IVG-3 | ell+grad+macro+loop+ctx+path         |
| tests/ivg/textTest1.ivg                                                              | IVG-2 | star+grad+mask+loop+ctx+text         |
| tests/ivg/trancelvaniaMin_logo.ivg                                                   | IVG-1 | grad+path                            |
| tests/ivg/trancelvania_logo.ivg                                                      | IVG-1 | grad+path                            |
| tests/ivg/transformTests1.ivg                                                        | IVG-1 | grad+mask+loop+ctx                   |
| tests/ivg/unicode.ivg                                                                | IVG-2 | loop+text+unicode                    |
| tests/ivg/vortex_logo.ivg                                                            | IVG-1 | grad+pat+path                        |
| tests/ivg/vortex_logo_full.ivg                                                       | N/A   | grad+pat+path                        |
| tests/svg/supported/blossom.ivg                                                      | IVG-3 | ctx+path                             |
| tests/svg/supported/blossomCSS.ivg                                                   | IVG-3 | ctx+path                             |
| tests/svg/supported/blossomStyles.ivg                                                | IVG-3 | ctx+path                             |
| tests/svg/supported/circle.ivg                                                       | IVG-3 | ell+ctx                              |
| tests/svg/supported/color-names.ivg                                                  | IVG-3 | ell+ctx                              |
| tests/svg/supported/defs-use.ivg                                                     | IVG-3 | ctx                                  |
| tests/svg/supported/ellipse.ivg                                                      | IVG-3 | ell+ctx                              |
| tests/svg/supported/gradient-radial.ivg                                              | IVG-3 | grad+ctx                             |
| tests/svg/supported/gradient-stops.ivg                                               | IVG-3 | grad+ctx                             |
| tests/svg/supported/gradient-transform.ivg                                           | IVG-3 | grad+ctx                             |
| tests/svg/supported/gradient.ivg                                                     | IVG-3 | grad+ctx                             |
| tests/svg/supported/group.ivg                                                        | IVG-3 | ell+ctx                              |
| tests/svg/supported/line.ivg                                                         | IVG-3 | ctx+path                             |
| tests/svg/supported/matrix.ivg                                                       | IVG-3 | ctx                                  |
| tests/svg/supported/multi-path.ivg                                                   | IVG-3 | ctx+path                             |
| tests/svg/supported/opacity.ivg                                                      | IVG-3 | ell+ctx                              |
| tests/svg/supported/path.ivg                                                         | IVG-3 | ctx+path                             |
| tests/svg/supported/percentage.ivg                                                   | IVG-3 | ctx                                  |
| tests/svg/supported/polygon.ivg                                                      | IVG-3 | ctx+path                             |
| tests/svg/supported/polyline.ivg                                                     | IVG-3 | ctx+path                             |
| tests/svg/supported/rect.ivg                                                         | IVG-3 | ctx                                  |
| tests/svg/supported/resvg_tests_masking_clipPath_clipPathUnits=objectBoundingBox.ivg | IVG-3 | mask+ctx                             |
| tests/svg/supported/resvg_tests_painting_color_inherit.ivg                           | IVG-3 | ctx                                  |
| tests/svg/supported/resvg_tests_painting_marker_marker-on-line.ivg                   | IVG-3 | ctx+path                             |
| tests/svg/supported/resvg_tests_shapes_rect_em-values.ivg                            | IVG-3 | ctx                                  |
| tests/svg/supported/resvg_tests_shapes_rect_vw-and-vh-values.ivg                     | IVG-3 | ctx                                  |
| tests/svg/supported/skew.ivg                                                         | IVG-3 | ctx                                  |
| tests/svg/supported/stroke-fill.ivg                                                  | IVG-3 | ctx+path                             |
| tests/svg/supported/text-stroke.ivg                                                  | IVG-3 | text                                 |
| tests/svg/supported/text.ivg                                                         | IVG-3 | text                                 |
| tests/svg/supported/transform.ivg                                                    | IVG-3 | ctx                                  |
| tests/svg/supported/units.ivg                                                        | IVG-3 | ctx                                  |
| tests/svg/supported/viewbox.ivg                                                      | IVG-3 | ctx                                  |
