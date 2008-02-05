// [Name] SVGCircleElement-dom-cy-attr.js
// [Expected rendering result] green circle - and a series of PASS mesages

description("Tests dynamic updates of the 'cy' attribute of the SVGCircleElement object")
createSVGTestCase();

var circleElement = createSVGElement("circle");
circleElement.setAttribute("cx", "150");
circleElement.setAttribute("cy", "-150");
circleElement.setAttribute("r", "150");
circleElement.setAttribute("fill", "green");

rootSVGElement.appendChild(circleElement);
shouldBeEqualToString("circleElement.getAttribute('cy')", "-150");

function executeTest() {
    circleElement.setAttribute("cy", "150");
    shouldBeEqualToString("circleElement.getAttribute('cy')", "150");

    waitForClickEvent(circleElement);
    triggerUpdate();
}

window.setTimeout("executeTest()", 0);
var successfullyParsed = true;
