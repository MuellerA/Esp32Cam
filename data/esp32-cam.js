////////////////////////////////////////////////////////////////////////////////

"use strict" ;

////////////////////////////////////////////////////////////////////////////////
// global vars
////////////////////////////////////////////////////////////////////////////////

var menuContent ;

////////////////////////////////////////////////////////////////////////////////
// menu
////////////////////////////////////////////////////////////////////////////////

function menu()
{
    menuContent.style.display = (menuContent.style.display == 'none') ? 'block' : 'none' ;
}

//window.onclick = function(event)
//{
//    if (!event.target.matches('.dropdown-button'))
//        menuContent.style.display = 'none' ;
//}

////////////////////////////////////////////////////////////////////////////////
// main()
////////////////////////////////////////////////////////////////////////////////

function main()
{
    menuContent = document.getElementById("menu-content") ;
}

////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////