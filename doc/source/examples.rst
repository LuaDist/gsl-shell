.. highlight:: lua

.. include:: <isogrk1.txt>

GSL Shell Examples
==================

In this chapter we gives some examples about the usage of GSL Shell.

Home-made Bessel Functions
--------------------------

The Bessel's function J\ :sub:`n` for integer values of n can de defined with the following integral:

.. math::
   J_n(x) = {1 \over \pi} \int_0^\pi \cos(n \tau - x \sin \tau) \textrm{d}\tau

We can use this definition to define our home-made Bessel function. To perform the integral we need to use the ``integ`` function and provide the function to integrate. This is easy like a piece of cake::

   function bessJ(x,n)
      local f = |t| cos(n*t - x*sin(t)) -- we define the function to integrate
      return 1/pi * integ {f= f, points={0, pi}}
   end

The definition of ``bessJ`` takes x and n as arguments and calculate the definite integral between 0 and |pgr|. Then we can calculate a matrix with the tabulated values. For examples we can use the columns of the matrix to span different values of n. We write then::

   m = new(200, 6, |k,n| bessJ((k-1)/10, n-1))

And we obtain the following matrix::

  [           1           0           0           0           0           0 ]
  [    0.997502   0.0499375  0.00124896 2.08203e-05 2.60286e-07           0 ]
  [    0.990025   0.0995008  0.00498335  0.00016625 4.15834e-06 8.31945e-08 ]
  [    0.977626    0.148319   0.0111659 0.000559343  2.0999e-05 6.30443e-07 ]
  [    0.960398    0.196027   0.0197347  0.00132005 6.61351e-05 2.64894e-06 ]
  [     0.93847    0.242268    0.030604  0.00256373 0.000160736 8.05363e-06 ]
  [    0.912005    0.286701   0.0436651  0.00439966  0.00033147 1.99482e-05 ]
  [    0.881201    0.328996   0.0587869  0.00692965 0.000610097 4.28824e-05 ]
  [    0.846287    0.368842   0.0758178   0.0102468  0.00103298 8.30836e-05 ]
  [    0.807524     0.40595   0.0945863    0.014434  0.00164055 0.000148658 ]
  [    0.765198    0.440051    0.114903   0.0195634  0.00247664 0.000249758 ]
  [ ... ]
